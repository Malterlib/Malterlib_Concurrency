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
				CRequest const &_Request
				, CChallenge const &_Challenge
				, CStr const &_MultipleRequestID
			) override
		;

		struct CAuthenticationState
		{
			TCPromise<TCVector<ICDistributedActorAuthenticationHandler::CResponse>> m_Promise;
			TCVector<ICDistributedActorAuthenticationHandler::CResponse> m_Responses;
			TCMap<CStr, bool> m_Tested;
			TCVector<CStr> m_FactorOrder;
			TCVector<CStr>::CIterator m_iCurrentFactor;
			TCSharedPointer<CPreauthenticationInfo> m_pInfo;
			TCSharedPointer<CCommandLineControl> m_pCommandLine;
			TCMap<CStr, CAuthenticationActorInfo> m_ActorInfo;
			TCMap<CStr, TCMap<CStr, CAuthenticationData>> m_UserFactors;
			TCMap<TCSet<CStr>, TCSet<int32>> m_Fulfills;
			int32 m_nRequirements;
			NTime::CTime m_ExpirationTime;
			NStr::CStr m_LastPrompt;
		};

		TCFuture<CMultipleRequestData> f_GetMultipleRequestSubscription(uint32 _nHosts) override;

	private:
		void fp_TryOneFactorAtATime(TCSharedPointer<CAuthenticationState> const &_pState);
		TCFuture<TCVector<CResponse>> fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> const &_pInfo);
		void fp_RequestCollected(TCSharedPointer<CPreauthenticationInfo> const &_pInfo);
		TCFuture<void> fp_Destroy() override;

		TCSharedPointer<CCommandLineControl> const mp_pCommandLine;
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
		TCMap<CStr, TCSharedPointer<CPreauthenticationInfo>> mp_PreauthenticationInfos;
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

	void CDistributedAppAuthenticationHandler::fp_TryOneFactorAtATime(TCSharedPointer<CAuthenticationState> const &_pState)
	{
		auto &State = *_pState;
		if (!State.m_iCurrentFactor)
		{
			State.m_Promise.f_SetResult(State.m_Responses);
			return;
		}

		auto &Info = *State.m_pInfo;

		// Which permissions needs this factor?
		TCSet<CStr> Permissions;
		TCMap<CStr, TCSet<CStr>> PermissionDescriptions;
		for (auto const &OuterRequestedPermissions : Info.m_Request.m_RequestedPermissions)
		{
			for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
			{
				if (PermissionWithRequirements.m_Preauthenticated.f_FindEqual(*State.m_iCurrentFactor))
					continue; // Even if this permission requires this factor, it has been preauthenticated and should not be listed

				for (auto const &Factors : PermissionWithRequirements.m_AuthenticationFactors)
				{
					if (Factors.f_FindEqual(*State.m_iCurrentFactor))
					{
						PermissionDescriptions[PermissionWithRequirements.m_Description][PermissionWithRequirements.m_Permission];
						Permissions[PermissionWithRequirements.m_Permission];
					}
				}
			}
		}

		auto Actor = State.m_ActorInfo[*State.m_iCurrentFactor].m_Actor;
		if (!Actor)
		{
			State.m_Promise.f_SetResult(State.m_Responses);
			return;
		}

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

		Actor
			(
				&ICDistributedActorTrustManagerAuthenticationActor::f_SignAuthenticationRequest
				, State.m_pCommandLine
				, Info.m_Request.m_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties{Permissions, Info.m_Challenges, State.m_ExpirationTime}
				, State.m_UserFactors[*State.m_iCurrentFactor]
			)
			> [=](TCAsyncResult<ICDistributedActorAuthenticationHandler::CResponse> &&_Response)
			{
				auto &State = *_pState;
				auto CurrentFactor = *State.m_iCurrentFactor;
				if (!_Response)
				{
					auto AnsiEncoding = State.m_pCommandLine->f_AnsiEncoding();

					*State.m_pCommandLine %= "Failed to authenticate with factor '{}': {}{}{}\n"_f
						<< CurrentFactor
						<< AnsiEncoding.f_StatusError()
						<< _Response.f_GetExceptionStr()
						<< AnsiEncoding.f_Default()
					;

					State.m_Tested[CurrentFactor] = false;
					++State.m_iCurrentFactor;
					fp_TryOneFactorAtATime(_pState);
					return;
				}

				if (!_Response->m_Signature.f_IsEmpty())
				{
					State.m_Responses.f_Insert(fg_Move(*_Response));
					State.m_Tested[CurrentFactor] = true;

					// Was this enough to authenticate all requirements?
					TCSet<int32> Fulfilled;
					for (auto &Members : State.m_Fulfills)
					{
						auto const &Factors = State.m_Fulfills.fs_GetKey(Members);
						bool bAllSucceeded = true;
						for (auto const &Factor : Factors)
						{
							auto *pValue = State.m_Tested.f_FindEqual(Factor);
							if (!pValue || !*pValue)
							{
								bAllSucceeded = false;
								break;
							}
						}
						if (bAllSucceeded)
							Fulfilled += Members;
					}
					if (Fulfilled.f_GetLen() == State.m_nRequirements)
					{
						State.m_Promise.f_SetResult(State.m_Responses);
						return;
					}
				}
				else
				{
					State.m_Tested[CurrentFactor] = false;
					// TODO: If the authentication failed we can detect that
					// A) we will never be able to authenticate and give up early
					// B) some other factors might be in AND set with the one that failed and there is no use asking for that factor
					// but as long as we only have two factors...
				}

				++State.m_iCurrentFactor;
				fp_TryOneFactorAtATime(_pState);
			}
		;
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

	auto CDistributedAppAuthenticationHandler::fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> const &_pInfo)
		-> TCFuture<TCVector<ICDistributedActorAuthenticationHandler::CResponse>>
	{
		TCSharedPointer<CAuthenticationState> pState = fg_Construct();
		auto &State = *pState;
		State.m_pCommandLine = mp_pCommandLine;
		State.m_pInfo = _pInfo;

		mp_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			+ mp_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _pInfo->m_UserID)
			> [this, pState, pCommandLine = mp_pCommandLine, _pInfo]
			(
				TCAsyncResult<TCMap<CStr, CAuthenticationActorInfo>> &&_ActorInfo
				, TCAsyncResult<TCMap<CStr, CAuthenticationData>> &&_RegisteredFactors
			) mutable
			{
				auto &State = *pState;
				if (!_ActorInfo)
				{
					State.m_Promise.f_SetException(_ActorInfo.f_GetException());
					return;
				}
				if (!_RegisteredFactors)
				{
					State.m_Promise.f_SetException(_RegisteredFactors.f_GetException());
					return;
				}
				auto &Info = *_pInfo;
				State.m_ActorInfo = fg_Move(*_ActorInfo);
				for (auto const &Data : *_RegisteredFactors)
				{
					State.m_UserFactors[Data.m_Name][_RegisteredFactors->fs_GetKey(Data)] = Data;
				}

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
								if (!State.m_ActorInfo.f_FindEqual(Factor) || !State.m_UserFactors.f_FindEqual(Factor))
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
							return State.m_Promise.f_SetResult(TCVector<ICDistributedActorAuthenticationHandler::CResponse>{});	// This permission will never be authenticated
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
						{
							State.m_Promise.f_SetException(DMibErrorInstance("Too many factors"));
							return;
						}

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

					COrdering_Strong operator <=> (CFulfillingFactors const &_Other) const
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
						FulfillingFactorSets[CFulfillingFactors{Factors, State.m_ActorInfo}];
				}

				// We have now found all fulfilling factors sets, sorted in smallest and least cumbersome order.
				// We will use the order from these sets to minimize the number of needed authentications.
				// Since we add the factor the factors in the set traversal order Password will come before U2F devices, and that is probably the way we want it.
				//
				// In the example we will first add A and B since that is a fulfilling set, and the add C, D, and E as a part of AllFactors.
				auto fAdd = [&Order = State.m_FactorOrder](TCSet<CStr> const &_Factors)
					{
						for (auto const &Factor : _Factors)
						{
							for (auto const &AlreadyAdded : Order)
							{
								if (Factor == AlreadyAdded)
									return;
							}
							Order.f_Insert(Factor);
						}
					}
				;
				for (auto const &Fulfilling : FulfillingFactorSets)
					fAdd(Fulfilling.m_Factors);
				fAdd(AllFactors);

				if (mp_AuthenticationLifetime != CPermissionRequirements::mc_OverrideLifetimeNotSet)
					State.m_ExpirationTime = NTime::CTime::fs_NowUTC() + NTime::CTimeSpanConvert::fs_CreateMinuteSpan(mp_AuthenticationLifetime);

				State.m_iCurrentFactor = State.m_FactorOrder.f_GetIterator();
				fp_TryOneFactorAtATime(pState);
			}
		;

		return pState->m_Promise.f_Future();
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
			CRequest const &_Request
			, CChallenge const &_Challenge
			, CStr const &_MultipleRequestID
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

		if (--Info.m_nHosts == 0)
			fp_RequestCollected(pInfo);
		else if (!Info.m_bTimerIsSet)
		{
			Info.m_bTimerIsSet = true;
			fg_Timeout(5.0) > [this, pInfo]() mutable
				{
					if (pInfo->m_bAuthenticationCompleted || pInfo->m_bResultsSet)
						return;
					fp_RequestCollected(pInfo);
				}
			;
		}

		auto Future = fg_Exchange(pInfo, nullptr)->m_Promises[HostID].f_Future();
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

	TCFuture<CActorSubscription> CDistributedAppActor::fp_SetupAuthentication
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, int64 _AuthenticationLifetime
			, CStr const &_UserID
		)
	{
		TCPromise<CActorSubscription> Promise;
		if (!mp_Settings.m_bSupportUserAuthentication)
			return Promise <<= g_Void;

		return Promise <<= self(&CDistributedAppActor::fp_EnableAuthentication, _pCommandLine, _AuthenticationLifetime, _UserID);
	}

	TCFuture<CActorSubscription> CDistributedAppActor::fp_EnableAuthentication
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, int64 _AuthenticationLifetime
			, CStr _UserID
		)
	{
		if (mp_AuthenticationHandlerImplementation)
			co_return DMibErrorInstance("Authentication already enabled");

		mp_AuthenticationHandlerImplementation
			= mp_State.m_DistributionManager->f_ConstructActor<CDistributedAppAuthenticationHandler>(_pCommandLine, mp_State.m_TrustManager, _AuthenticationLifetime)
		;

		auto Subscription = g_ActorSubscription / [=]() -> TCFuture<void>
			{
				TCActorResultVector<void> Destroys;

				mp_AuthenticationRemotes.f_Destroy() > Destroys.f_AddResult();

				for (auto &Subscription : mp_AuthenticationRegistrationSubscriptions)
				{
					if (Subscription.m_Subscription)
						Subscription.m_Subscription->f_Destroy() > Destroys.f_AddResult();;
				}
				mp_AuthenticationRegistrationSubscriptions.f_Clear();

				if (mp_AuthenticationHandlerImplementation)
					fg_Move(mp_AuthenticationHandlerImplementation).f_Destroy() > Destroys.f_AddResult();

				co_await Destroys.f_GetUnwrappedResults().f_Wrap() > fg_LogWarning("Mib/Concurrency/App", "Failed to destroy authentication subscription");

				co_return {};
			}
		;

		CStr DefaultUserID = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser);

		CStr UserID = _UserID;
		if (UserID.f_IsEmpty())
			UserID = DefaultUserID;

		if (!UserID)	// No use setting up authentication without a default user
			co_return {};

		mp_AuthenticationRemotes = co_await mp_State.m_TrustManager->f_SubscribeTrustedActors<ICDistributedActorAuthentication>();

		co_await mp_AuthenticationRemotes.f_OnActor
			(
				g_ActorFunctor / [this, UserID](TCDistributedActor<ICDistributedActorAuthentication> const &_NewActor, CTrustedActorInfo const &_ActorInfo) -> TCFuture<void>
				{
					mp_AuthenticationRegistrationSubscriptions[_NewActor].m_ActorInfo = _ActorInfo;

					auto Subscription = co_await _NewActor.f_CallActor(&ICDistributedActorAuthentication::f_RegisterAuthenticationHandler)
						(
							mp_AuthenticationHandlerImplementation->f_ShareInterface<ICDistributedActorAuthenticationHandler>()
							, UserID
						)
						.f_Wrap()
					;

					if (!Subscription)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/App
								, Error
								, "Registration of authentication handler interface for server {} failed: {}"
								, _ActorInfo.m_HostInfo.f_GetDesc()
								, Subscription.f_GetExceptionStr()
							)
						;

						co_return Subscription.f_GetException();
					}

					DMibLogWithCategory(Mib/Concurrency/App, Info, "Successfully registered authentication handler for remote {}", _ActorInfo.m_HostInfo.f_GetDesc());

					if (auto pSubscription = mp_AuthenticationRegistrationSubscriptions.f_FindEqual(_NewActor))
						pSubscription->m_Subscription = fg_Move(*Subscription);

					co_return {};
				}
				, g_ActorFunctor / [this](TCWeakDistributedActor<CActor> const &_RemovedActor, CTrustedActorInfo &&_ActorInfo) -> TCFuture<void>
				{
					mp_AuthenticationRegistrationSubscriptions.f_Remove(_RemovedActor);

					co_return {};
				}
			)
		;

		co_return fg_Move(Subscription);
	}
}

