// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include <Mib/Concurrency/DistributedApp> // For CCommandLineControl
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/Concurrency/DistributedActorAuthenticationHandler>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NContainer;

	struct CDistributedAppAuthenticationHandler : public ICDistributedActorAuthenticationHandler
	{
		struct CPreauthenticationInfo
		{
			CRequest m_Request;
			CStr m_UserID;
			TCMap<CStr, CChallenge> m_Challenges;
			TCMap<CStr, TCPromise<TCVector<CResponse>>> m_Promises;
			mint m_nHosts = 1;
			bool m_bInitialized = false;
			bool m_bTimerIsSet = false;
			bool m_bAuthenticationCompleted = false;
			bool m_bResultsSet = false;
		};

		CDistributedAppAuthenticationHandler
			(
				TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, TCActor<CDistributedActorTrustManager> const &_TrustManager
				, int64 _AuthenticationLifetime
			)
		;

		TCFuture<TCVector<CResponse>> f_RequestAuthentication
			(
				CRequest _Request
				, CChallenge _Challenge
				, CStr _MultipleRequestID
			) override
		;

		struct CAuthenticationStateFactor
		{
			CAuthenticationActorInfo m_ActorInfo;
			TCMap<CStr, CAuthenticationData> m_UserFactors;
			TCOptional<bool> m_Tested;
		};

		struct CAuthenticationState
		{
			TCVector<ICDistributedActorAuthenticationHandler::CResponse> m_Responses;
			TCMap<CStr, CAuthenticationStateFactor> m_FactorStates;
			TCSharedPointer<CPreauthenticationInfo> m_pInfo;
			TCSharedPointer<CCommandLineControl> m_pCommandLine;

			TCMap<TCSet<CStr>, TCSet<int32>> m_Fulfills;
			int32 m_nRequirements;
			NTime::CTime m_ExpirationTime;
			NStr::CStr m_LastPrompt;
		};

		TCFuture<CMultipleRequestData> f_GetMultipleRequestSubscription(uint32 _nHosts) override;

	private:
		TCFuture<void> fp_TryOneFactor(TCSharedPointer<CAuthenticationState> _pState, CStr _Factor);
		TCFuture<TCVector<CResponse>> fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> _pInfo);
		void fp_RequestCollected(TCSharedPointer<CPreauthenticationInfo> const &_pInfo);
		TCFuture<void> fp_Destroy() override;

		TCSharedPointer<CCommandLineControl> const mp_pCommandLine;
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		TCMap<CStr, TCSharedPointer<CPreauthenticationInfo>> mp_PreauthenticationInfos;
		TCSequencer<void> mp_PromptSequencer{"AuthenticationHandler"};
		int64 mp_AuthenticationLifetime;
	};

	CDistributedAppAuthenticationHandler::CDistributedAppAuthenticationHandler
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, int64 _AuthenticationLifetime
		)
		: mp_pCommandLine(_pCommandLine)
		, mp_TrustManager(_TrustManager)
		, mp_AuthenticationLifetime(_AuthenticationLifetime)
	{
	}

	TCFuture<void> CDistributedAppAuthenticationHandler::fp_Destroy()
	{
		for (auto &pAuthenticationInfo : mp_PreauthenticationInfos)
		{
			if (pAuthenticationInfo->m_bResultsSet)
				continue;
			pAuthenticationInfo->m_bResultsSet = true;

			for (auto &Promise : pAuthenticationInfo->m_Promises)
				Promise.f_SetException(DMibErrorInstance("Authentication handler destroyed"));
		}
		mp_PreauthenticationInfos.f_Clear();
		co_return {};
	}

	TCFuture<void> CDistributedAppAuthenticationHandler::fp_TryOneFactor
		(
			TCSharedPointer<CAuthenticationState> _pState
			, CStr _Factor
		)
	{
		auto &State = *_pState;
		auto &Info = *State.m_pInfo;
		auto &FactorState = State.m_FactorStates[_Factor];

		// Which permissions needs this factor?
		TCSet<CStr> Permissions;
		TCMap<CStr, TCSet<CStr>> PermissionDescriptions;
		for (auto const &OuterRequestedPermissions : Info.m_Request.m_RequestedPermissions)
		{
			for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
			{
				if (PermissionWithRequirements.m_Preauthenticated.f_FindEqual(_Factor))
					continue; // Even if this permission requires this factor, it has been preauthenticated and should not be listed

				for (auto const &Factors : PermissionWithRequirements.m_AuthenticationFactors)
				{
					if (Factors.f_FindEqual(_Factor))
					{
						PermissionDescriptions[PermissionWithRequirements.m_Description][PermissionWithRequirements.m_Permission];
						Permissions[PermissionWithRequirements.m_Permission];
					}
				}
			}
		}

		auto Actor = FactorState.m_ActorInfo.m_Actor;
		if (!Actor)
			co_return {};

		NStr::CStr Prompt = "\nAuthentication required for '{}'. Please authenticate to allow the following permissions:\n"_f << Info.m_Request.m_Description;

		for (auto const &Permissions : PermissionDescriptions)
		{
			Prompt += "\n";
			auto const &Description = PermissionDescriptions.fs_GetKey(Permissions);
			if (Description.f_IsEmpty())
			{
				for (auto &Permission : Permissions)
					Prompt += "    {}\n"_f << Permission;
			}
			else
			{
				Prompt += "    {}:\n"_f << Description;
				for (auto &Permission : Permissions)
					Prompt += "        {}\n"_f << Permission;
			}
		}

		Prompt += "\n";

		if (Prompt != State.m_LastPrompt)
		{
			State.m_LastPrompt = Prompt;
			*State.m_pCommandLine %= Prompt;
		}


		auto Response = co_await Actor
			(
				&ICDistributedActorTrustManagerAuthenticationActor::f_SignAuthenticationRequest
				, State.m_pCommandLine
				, Info.m_Request.m_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties{Permissions, Info.m_Challenges, State.m_ExpirationTime}
				, FactorState.m_UserFactors
			)
			.f_Wrap()
		;

		auto CurrentFactor = _Factor;
		if (!Response)
		{
			auto AnsiEncoding = State.m_pCommandLine->f_AnsiEncoding();

			*State.m_pCommandLine %= "Failed to authenticate with factor '{}': {}{}{}\n"_f
				<< CurrentFactor
				<< AnsiEncoding.f_StatusError()
				<< Response.f_GetExceptionStr()
				<< AnsiEncoding.f_Default()
			;

			FactorState.m_Tested = false;
			co_return {};
		}

		if (!Response->m_Signature.f_IsEmpty())
		{
			State.m_Responses.f_Insert(fg_Move(*Response));
			FactorState.m_Tested = true;
		}
		else
		{
			FactorState.m_Tested = false;
			// TODO: If the authentication failed we can detect that
			// A) we will never be able to authenticate and give up early
			// B) some other factors might be in AND set with the one that failed and there is no use asking for that factor
			// but as long as we only have two factors...
		}

		co_return {};
	}

	template <typename tf_CValue>
	static TCVector<TCSet<tf_CValue>> fg_PowerSet(TCSet<tf_CValue> const &_SourceSet)
	{
		if (_SourceSet.f_IsEmpty())
			return {};

		TCVector<TCSet<tf_CValue>> PowerSet;
		TCVector<tf_CValue> SourceSetArray;
		for (auto &Value : _SourceSet)
			SourceSetArray.f_Insert(Value);

		DMibFastCheck(!SourceSetArray.f_IsEmpty());

		mint nSubSets = _SourceSet.f_GetLen();
		mint nElements = mint(1) << nSubSets;

		for (mint Set = 1; Set < nElements; ++Set)
		{
			mint iBit = 0;

			TCSet<tf_CValue> SubSet;

			mint SetBits = Set;
			while (SetBits)
			{
				mint iLowestSet = fg_GetLowestBitSetNoZero(SetBits);
				iBit += iLowestSet;

				SubSet[SourceSetArray[iBit]];

				SetBits = SetBits >> (iLowestSet + 1);
				++iBit;
			}

			PowerSet.f_Insert(SubSet);
		}
		return PowerSet;
	}

	auto CDistributedAppAuthenticationHandler::fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> _pInfo)
		-> TCFuture<TCVector<ICDistributedActorAuthenticationHandler::CResponse>>
	{
		TCSharedPointer<CAuthenticationState> pState = fg_Construct();
		auto &State = *pState;
		State.m_pCommandLine = mp_pCommandLine;
		State.m_pInfo = _pInfo;

		auto SequencerSubscription = co_await mp_PromptSequencer.f_Sequence();

		auto [ActorInfo, RegisteredFactors] = co_await
			(
				mp_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
				+ mp_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _pInfo->m_UserID)
			)
		;

		auto &Info = *_pInfo;
		for (auto &AuthenticationActorEntry : ActorInfo.f_Entries())
			State.m_FactorStates[AuthenticationActorEntry.f_Key()].m_ActorInfo = AuthenticationActorEntry.f_Value();

		for (auto const &Entry : RegisteredFactors.f_Entries())
			State.m_FactorStates[Entry.f_Value().m_Name].m_UserFactors[Entry.f_Key()] = Entry.f_Value();

		// Compute which of the named queries that are fulfilled by the occuring factor combinations
		//
		// For example:
		// 1: (A & B) | C
		// 2: A       | (D & E)
		// 3: B       | E
		// will produce:
		// { A }    : { 2 }
		// { A, B } : { 1 }
		// { B }    : { 3 }
		// { C }    : { 1 }
		// { D, E } : { 2 }
		// { E }    : { 3 }
		int32 nRequirement = 0;
		for (auto const &OuterRequestedPermissions : Info.m_Request.m_RequestedPermissions)
		{
			++nRequirement;
			for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
			{
				bool bCanHandleAtLeastOne = false;
				for (auto const &FactorGroup : PermissionWithRequirements.m_AuthenticationFactors)
				{
					bool bCanHandleAll = true;
					auto NeedsAuthentication = FactorGroup;
					NeedsAuthentication -= PermissionWithRequirements.m_Preauthenticated;

					for (auto Factor : NeedsAuthentication)
					{
						auto pFatorState = State.m_FactorStates.f_FindEqual(Factor);
						if (!pFatorState || !pFatorState->m_ActorInfo.m_Actor || pFatorState->m_UserFactors.f_IsEmpty())
						{
							bCanHandleAll = false;
							break;
						}
					}
					if (bCanHandleAll)
					{
						State.m_Fulfills[NeedsAuthentication][nRequirement];
						bCanHandleAtLeastOne = true;
					}
				}
				if (!bCanHandleAtLeastOne)
					co_return TCVector<ICDistributedActorAuthenticationHandler::CResponse>{};	// This permission will never be authenticated
			}
		}
		State.m_nRequirements = nRequirement;

		// Iterate through all collected factor sets and add the members for subsets, they are handled for free if we choose the superset
		//
		// After this operation we will get
		// { A }    : { 2 }
		// { A, B } : { 1, 2, 3 }
		// { B }    : { 3 }
		// { C }    : { 1 }
		// { D, E } : { 2, 3 }
		// { E }    : { 3 }
		for (auto &Members : State.m_Fulfills)
		{
			auto const &Factors = State.m_Fulfills.fs_GetKey(Members);
			mint nFactors = Factors.f_GetLen();
			if (nFactors > 1)
			{
				if (nFactors > 16)
					co_return DMibErrorInstance("Too many factors");

				for (auto const &SubSet : fg_PowerSet(Factors))
				{
					if (SubSet == Factors)
						continue;
					if (auto *pSubSetMembers = State.m_Fulfills.f_FindEqual(SubSet))
						Members += *pSubSetMembers;
				}
			}
		}

		struct CFulfillingFactors
		{
			CFulfillingFactors(TCSet<CStr> const &_Factors, TCMap<CStr, CAuthenticationActorInfo> const &_ActorInfo)
				: m_Factors(_Factors)
			{
				for (auto const &Factor : m_Factors)
					m_Worst = fg_Max(m_Worst, _ActorInfo[Factor].m_Category);
			}

			COrdering_Strong operator <=> (CFulfillingFactors const &_Other) const noexcept
			{
				// A smaller set of factors is better than a larger set
				auto LeftLen = m_Factors.f_GetLen();
				auto RightLen = _Other.m_Factors.f_GetLen();
				if (auto Result = LeftLen <=> RightLen; Result != 0)
					return Result;

				// If they have the same cardinality, select the one with the lowest category.
				// This means a set without a password factor is selected before a set with a password factor
				if (auto Result = m_Worst <=> _Other.m_Worst; Result != 0)
					return Result;

				// Fall back to the regular TCSet compare operation as a tie breaker
				return m_Factors <=> _Other.m_Factors;
			}

			TCSet<CStr> m_Factors;
			EAuthenticationFactorCategory m_Worst = EAuthenticationFactorCategory_None;
		};

		TCSet<CStr> AllFactors;
		TCSet<CFulfillingFactors> FulfillingFactorSets;

		for (auto const &Members : State.m_Fulfills)
		{
			auto const &Factors = State.m_Fulfills.fs_GetKey(Members);
			AllFactors += Factors;

			if (Members.f_GetLen() == nRequirement) // Does the factor combination solve all the permission requirements?
				FulfillingFactorSets[CFulfillingFactors{Factors, ActorInfo}];
		}

		// We have now found all fulfilling factors sets, sorted in smallest and least cumbersome order.
		// We will use the order from these sets to minimize the number of needed authentications.
		// Since we add the factor the factors in the set traversal order Password will come before U2F devices, and that is probably the way we want it.
		//
		// In the example we will first add A and B since that is a fulfilling set, and the add C, D, and E as a part of AllFactors.

		TCVector<CStr> FactorOrder;

		auto fAdd = [&FactorOrder](TCSet<CStr> const &_Factors)
			{
				for (auto const &Factor : _Factors)
				{
					for (auto const &AlreadyAdded : FactorOrder)
					{
						if (Factor == AlreadyAdded)
							return;
					}
					FactorOrder.f_Insert(Factor);
				}
			}
		;
		for (auto const &Fulfilling : FulfillingFactorSets)
			fAdd(Fulfilling.m_Factors);
		fAdd(AllFactors);

		if (mp_AuthenticationLifetime != CPermissionRequirements::mc_OverrideLifetimeNotSet)
			State.m_ExpirationTime = NTime::CTime::fs_NowUTC() + NTime::CTimeSpanConvert::fs_CreateMinuteSpan(mp_AuthenticationLifetime);

		for (auto &Factor : FactorOrder)
		{
			co_await fp_TryOneFactor(pState, Factor);

			// Was this enough to authenticate all requirements?
			TCSet<int32> Fulfilled;
			for (auto &Members : State.m_Fulfills)
			{
				auto const &Factors = State.m_Fulfills.fs_GetKey(Members);
				bool bAllSucceeded = true;
				for (auto const &Factor : Factors)
				{
					auto pFactorState = State.m_FactorStates.f_FindEqual(Factor);
					if (!pFactorState || !pFactorState->m_Tested || !(*pFactorState->m_Tested))
					{
						bAllSucceeded = false;
						break;
					}
				}
				if (bAllSucceeded)
					Fulfilled += Members;
			}
			if (Fulfilled.f_GetLen() == State.m_nRequirements)
				co_return fg_Move(pState->m_Responses);
		}

		co_return fg_Move(pState->m_Responses);
	}

	void CDistributedAppAuthenticationHandler::fp_RequestCollected(TCSharedPointer<CPreauthenticationInfo> const &_pInfo)
	{
		auto &Info = *_pInfo;
		Info.m_bAuthenticationCompleted = true;
		fp_RequestAuthentication(_pInfo) > [_pInfo](TCAsyncResult<TCVector<CResponse>> &&_Results)
			{
				auto &Info = *_pInfo;
				if (Info.m_bResultsSet)
					return;
				Info.m_bResultsSet = true;

				if (!_Results)
				{
					for (auto &Promise : Info.m_Promises)
						Promise.f_SetException(_Results.f_GetException());
					return;
				}
				for (auto &Promise : Info.m_Promises)
					Promise.f_SetResult(_Results);
				Info.m_Promises.f_Clear();
			}
		;
	}

	TCFuture<TCVector<ICDistributedActorAuthenticationHandler::CResponse>> CDistributedAppAuthenticationHandler::f_RequestAuthentication
		(
			CRequest _Request
			, CChallenge _Challenge
			, CStr _MultipleRequestID
		)
	{
		if (f_IsDestroyed())
			co_return DMibErrorInstance("Authentication handler destroyed");

		TCSharedPointer<CPreauthenticationInfo> pInfo;
		if (_MultipleRequestID.f_IsEmpty())
		{
			// This function handle two cases of authentication. The normal case is single server request, initiated by the server. Those requests have no preshared multiple request ID,
			// so we generate one here and call fp_RequestCollected directly.
			//
			// The other case is when the command  line client tells the servers to pre-authenticate a pattern. In that case f_GetMultipleRequestSubscription is called to generate a
			// subscription and a preshared multiple request ID that will be supplied by the servers. Once we have a call from each server fp_RequestCollected is called.
			pInfo = fg_Construct();
		}
		else
		{
			auto *pMultipleInfo = mp_PreauthenticationInfos.f_FindEqual(_MultipleRequestID);
			if (!pMultipleInfo)
				co_return DMibErrorInstance("No such multiple request ID exists");
			pInfo = *pMultipleInfo;
			auto &Info = *pInfo;

			if (Info.m_bAuthenticationCompleted || Info.m_bResultsSet) // Ooops. Too late
				co_return {};
		}

		auto &Info = *pInfo;

		auto const &HostID = fg_GetCallingHostID();
		Info.m_Challenges[HostID] = _Challenge;
		if (!Info.m_bInitialized)
		{
			Info.m_Request = _Request;
			Info.m_UserID = _Challenge.m_UserID;
			Info.m_bInitialized = true;
		}
		else
		{
			if (Info.m_Request != _Request)
				co_return DMibErrorInstance("Internal error - request mismatch between requests");
			if (Info.m_UserID != _Challenge.m_UserID)
				co_return DMibErrorInstance("Internal error - UserID mismatch between requests");
		}

		auto Future = Info.m_Promises[HostID].f_Future();

		if (--Info.m_nHosts == 0)
			fp_RequestCollected(pInfo);
		else if (!Info.m_bTimerIsSet)
		{
			Info.m_bTimerIsSet = true;
			fg_Timeout(5.0) > [this, pInfo]() mutable -> TCFuture<void>
				{
					if (pInfo->m_bAuthenticationCompleted || pInfo->m_bResultsSet)
						co_return {};

					fp_RequestCollected(pInfo);

					co_return {};
				}
			;
		}

		pInfo.f_Clear();

		co_return co_await fg_Move(Future);
	}

	TCFuture<CDistributedAppAuthenticationHandler::CMultipleRequestData> CDistributedAppAuthenticationHandler::f_GetMultipleRequestSubscription(uint32 _nHosts)
	{
		if (_nHosts == 0)
			co_return DMibErrorInstance("You have to have at least one host to request subscription for");
		if (f_IsDestroyed())
			co_return DMibErrorInstance("Authentication handler destroyed");

		auto MultipleRequestID = NCryptography::fg_RandomID(mp_PreauthenticationInfos);
		auto &Info = *(mp_PreauthenticationInfos[MultipleRequestID] = fg_Construct());
		Info.m_nHosts = _nHosts;

		TCActorSubscriptionWithID<> Subscription = g_ActorSubscription / [this, MultipleRequestID]() -> TCFuture<void>
			{
				if (auto pInfo = mp_PreauthenticationInfos.f_FindEqual(MultipleRequestID))
				{
					auto &Info = **pInfo;
					if (!Info.m_bResultsSet)
					{
						Info.m_bResultsSet = true;
						for (auto &Promise : Info.m_Promises)
							Promise.f_SetException(DMibErrorInstance("Multiple authentication request abandoned"));
					}
					mp_PreauthenticationInfos.f_Remove(pInfo);
				}
				co_return {};
			}
		;
#ifndef DMibClangAnalyzerWorkaround
		co_return CMultipleRequestData{fg_Move(Subscription), fg_Move(MultipleRequestID)};
#else
		co_return DMibErrorInstance("Workaround analyzer bug");
#endif
	}

	TCFuture<CDistributedAppActor::CAuthenticationSetupResult> CDistributedAppActor::fp_SetupAuthentication
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, int64 _AuthenticationLifetime
			, CStr _UserID
		)
	{
		if (!mp_Settings.m_bSupportUserAuthentication)
			return CAuthenticationSetupResult{};

		return fp_EnableAuthentication(_pCommandLine, _AuthenticationLifetime, _UserID);
	}

	TCFuture<CDistributedAppActor::CAuthenticationSetupResult> CDistributedAppActor::fp_EnableAuthentication
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, int64 _AuthenticationLifetime
			, CStr _UserID
		)
	{
		// Generate handler ID on distributed app side
		uint32 HandlerID = mp_NextAuthenticationHandlerID++;

		// Check if this handler is first (assumes "onlyship" for legacy remote compatibility)
		bool bAssumeOnlyHandler = mp_AuthenticationHandlers.f_IsEmpty();

		// Create authentication handler state
		auto &HandlerState = mp_AuthenticationHandlers[HandlerID];
		HandlerState.m_Handler
			= mp_State.m_DistributionManager->f_ConstructActor<CDistributedAppAuthenticationHandler>(_pCommandLine, mp_State.m_TrustManager, _AuthenticationLifetime)
		;

		auto Subscription = g_ActorSubscription / [=, this]() -> TCFuture<void>
			{
				auto *pState = mp_AuthenticationHandlers.f_FindEqual(HandlerID);
				if (!pState)
					co_return {};

				// Cleanup this handler's state
				TCFutureVector<void> Destroys;

				pState->m_AuthenticationRemotes.f_Destroy() > Destroys;

				for (auto &Sub : pState->m_RegistrationSubscriptions)
				{
					if (Sub.m_Subscription)
						Sub.m_Subscription->f_Destroy() > Destroys;
				}

				if (pState->m_Handler)
					fg_Move(pState->m_Handler).f_Destroy() > Destroys;

				mp_AuthenticationHandlers.f_Remove(pState);

				co_await fg_AllDone(Destroys).f_Wrap() > fg_LogWarning("Mib/Concurrency/App", "Failed to destroy authentication handler {}"_f << HandlerID);

				co_return {};
			}
		;

		CStr DefaultUserID = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser);

		CStr UserID = _UserID;
		if (UserID.f_IsEmpty())
			UserID = DefaultUserID;

		if (!UserID) // No use setting up authentication without a user
			co_return CAuthenticationSetupResult{};

		auto *pHandlerState = mp_AuthenticationHandlers.f_FindEqual(HandlerID);
		if (!pHandlerState)
			co_return CAuthenticationSetupResult{};

		pHandlerState->m_AuthenticationRemotes = co_await mp_State.m_TrustManager->f_SubscribeTrustedActors<ICDistributedActorAuthentication>();

		co_await pHandlerState->m_AuthenticationRemotes.f_OnActor
			(
				g_ActorFunctor / [this, UserID, HandlerID, bAssumeOnlyHandler](TCDistributedActor<ICDistributedActorAuthentication> _NewActor, CTrustedActorInfo _ActorInfo) -> TCFuture<void>
				{
					auto *pState = mp_AuthenticationHandlers.f_FindEqual(HandlerID);
					if (!pState)
						co_return {};

					pState->m_RegistrationSubscriptions[_NewActor].m_ActorInfo = _ActorInfo;

					uint32 InterfaceVersion = _NewActor->f_InterfaceVersion();
					if (InterfaceVersion < ICDistributedActorAuthentication::EProtocolVersion_AddHandlerID)
					{
						if (!bAssumeOnlyHandler)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/App
									, Warning
									, "Remote {} doesn't support multiple authentication handlers, ignoring registration"
									, _ActorInfo.m_HostInfo.f_GetDesc()
								)
							;
							co_return {};
						}
					}

					ICDistributedActorAuthentication::CRegisterAuthenticationHandlerParams Params;
					Params.m_Handler = pState->m_Handler->f_ShareInterface<ICDistributedActorAuthenticationHandler>();
					Params.m_UserID = UserID;
					Params.m_HandlerID = HandlerID;

					auto Subscription = co_await _NewActor.f_CallActor(&ICDistributedActorAuthentication::f_RegisterAuthenticationHandler)(fg_Move(Params)).f_Wrap();

					if (!Subscription)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/App
								, Error
								, "Registration of authentication handler {} interface for server {} failed: {}"
								, HandlerID
								, _ActorInfo.m_HostInfo.f_GetDesc()
								, Subscription.f_GetExceptionStr()
							)
						;

						co_return Subscription.f_GetException();
					}

					DMibLogWithCategory(Mib/Concurrency/App, Info, "Successfully registered authentication handler {} for remote {}", HandlerID, _ActorInfo.m_HostInfo.f_GetDesc());

					pState = mp_AuthenticationHandlers.f_FindEqual(HandlerID);
					if (pState)
					{
						if (auto pSubState = pState->m_RegistrationSubscriptions.f_FindEqual(_NewActor))
							pSubState->m_Subscription = fg_Move(*Subscription);
					}

					co_return {};
				}
				, g_ActorFunctor / [this, HandlerID](TCWeakDistributedActor<CActor> _RemovedActor, CTrustedActorInfo _ActorInfo) -> TCFuture<void>
				{
					auto *pState = mp_AuthenticationHandlers.f_FindEqual(HandlerID);
					if (pState)
						pState->m_RegistrationSubscriptions.f_Remove(_RemovedActor);

					co_return {};
				}
			)
		;

		co_return CAuthenticationSetupResult{fg_Move(Subscription), HandlerID};
	}
}

