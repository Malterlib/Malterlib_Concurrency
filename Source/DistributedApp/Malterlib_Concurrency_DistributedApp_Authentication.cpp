// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/DistributedApp> // For CCommandLineControl
#include <Mib/Concurrency/ActorSubscription>

#include <Mib/Concurrency/DistributedActorAuthenticationHandler>

namespace NMib::NConcurrency
{
	using namespace NPtr;
	using namespace NStr;
	using namespace NContainer;


	struct CDistributedAppAuthenticationHandler : public ICDistributedActorAuthenticationHandler
	{
		struct CPreauthenticationInfo
		{
			CRequest m_Request;
			CStr m_UserID;
			TCMap<CStr, CChallenge> m_Challenges;
			TCMap<CStr, TCContinuation<TCVector<CResponse>>> m_Continuations;
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

		TCContinuation<TCVector<CResponse>> f_RequestAuthentication
			(
				CRequest const &_Request
				, CStr const &_UserID
				, CChallenge const &_Challenge
				, CStr const &_MultipleRequestID
			) override
		;

		TCContinuation<CMultipleRequestData> f_GetMultipleRequestSubscription(uint32 _nHosts) override;

	private:
		TCContinuation<TCMap<CStr, TCVector<CResponse>>> fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> const &_pInfo);
		void fp_RequestCollected(TCSharedPointer<CPreauthenticationInfo> const &_pInfo);
		TCContinuation<void> fp_Destroy() override;

		NPtr::TCSharedPointer<CCommandLineControl> const mp_pCommandLine;
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

	TCContinuation<void> CDistributedAppAuthenticationHandler::fp_Destroy()
	{
		for (auto &pAuthenticationInfo : mp_PreauthenticationInfos)
		{
			if (pAuthenticationInfo->m_bResultsSet)
				continue;
			pAuthenticationInfo->m_bResultsSet = true;

			for (auto &Continuation : pAuthenticationInfo->m_Continuations)
				Continuation.f_SetException(DMibErrorInstance("Authentication handler destroyed"));
		}
		mp_PreauthenticationInfos.f_Clear();
		return fg_Explicit();
	}

	auto CDistributedAppAuthenticationHandler::fp_RequestAuthentication(TCSharedPointer<CPreauthenticationInfo> const &_pInfo)
		-> TCContinuation<TCMap<CStr, TCVector<ICDistributedActorAuthenticationHandler::CResponse>>>
	{
		TCContinuation<TCMap<CStr, TCVector<ICDistributedActorAuthenticationHandler::CResponse>>> Continuation;

		mp_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			+ mp_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _pInfo->m_UserID)
			> [this, Continuation, pCommandLine = mp_pCommandLine, _pInfo]
			(
			 	TCAsyncResult<TCMap<NStr::CStr, CAuthenticationActorInfo>> &&_ActorInfo
			 	, TCAsyncResult<TCMap<NStr::CStr, CAuthenticationData>> &&_RegisteredFactors
			)
			{
				if (!_ActorInfo)
				{
					Continuation.f_SetException(_ActorInfo.f_GetException());
					return;
				}
				if (!_RegisteredFactors)
				{
					Continuation.f_SetException(_RegisteredFactors.f_GetException());
					return;
				}
				auto &Info = *_pInfo;

				TCMap<CStr, TCMap<CStr, CAuthenticationData>> UserFactors;
				for (auto const &Data : *_RegisteredFactors)
					UserFactors[Data.m_Name][_RegisteredFactors->fs_GetKey(Data)] = Data;

				TCMap<TCSet<CStr>, TCSet<mint>> Fulfills;
				mint nRequirement = 0;
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
								if (!_ActorInfo->f_FindEqual(Factor) || !UserFactors.f_FindEqual(Factor))
								{
									bCanHandleAll = false;
									break;
								}
							}
							if (bCanHandleAll)
							{
								Fulfills[NeedsAuthentication][nRequirement];
								bCanHandleAtLeastOne = true;
							}
						}
						if (!bCanHandleAtLeastOne)
							return Continuation.f_SetResult(TCMap<CStr, TCVector<ICDistributedActorAuthenticationHandler::CResponse>>{});	// This permission will never be authenticated
					}
				}

				// Iterate through all collected factor sets and add the members for subsets, they are handled for free if we choose the superset
				for (auto &Members : Fulfills)
				{
					auto const &Factors = Fulfills.fs_GetKey(Members);
					if (Factors.f_GetLen() > 1)
					{
						// Here we should iterate through the full powerset. As long as we limit ourselves to TWO factor authentication it is enough to go through the individual
						// members of the set, but once we start using more than two factors, say (A, B, C) we should look for (A, B), (A,C), and (B, C) as well.
						for (auto const &Factor : Factors)
						{
							if (auto *pSubSetMembers = Fulfills.f_FindEqual(TCSet<CStr>{Factor}))
								Members += *pSubSetMembers;
						}
					}
				}

				TCSet<CStr> BestFactors;
				TCSet<CStr> AllFactors;
				auto fIsPrefered = [_ActorInfo, &BestFactors](TCSet<CStr> const &_Factors) -> bool
					{
						// We will only come here when the two sets have the same cardinality. Select the one with the lowest category. (I.e. Try to avoid asking for password).

						EAuthenticationFactorCategory Worst1 = EAuthenticationFactorCategory_Knowledge;
						EAuthenticationFactorCategory Worst2 = EAuthenticationFactorCategory_Knowledge;
						for (auto const &Factor : BestFactors)
							Worst1 = fg_Min(Worst1, (*_ActorInfo)[Factor].m_Category);
						for (auto const &Factor : _Factors)
							Worst2 = fg_Min(Worst2, (*_ActorInfo)[Factor].m_Category);

						return Worst1 > Worst2;
					}
				;
				for (auto const &Members : Fulfills)
				{
					auto const &Factors = Fulfills.fs_GetKey(Members);
					AllFactors += Factors;

					if (Members.f_GetLen() == nRequirement) // Does the factor combination solve all the permission requirements?
					{
						auto nBestMembers = BestFactors.f_GetLen();
						auto nMembers = Factors.f_GetLen();

						if (nBestMembers == 0 || nBestMembers > nMembers || (nBestMembers == nMembers && fIsPrefered(Factors)))
							BestFactors = Factors;
					}
				}
				if (BestFactors.f_IsEmpty())
				{
					// Failed to find a minimal combination of Factors that can be used to authenticate all permissions
					// Iterate through all Factors and filter out those permissions already handled. Most likely not the optimal solution, but it works for now.
					BestFactors = AllFactors;
				}

				TCActorResultVector<NContainer::TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> AuthenticationResults;

				NTime::CTime ExpirationTime;
				if (mp_AuthenticationLifetime != CPermissionRequirements::mc_OverrideLifetimeNotSet)
					ExpirationTime = NTime::CTime::fs_NowUTC() + NTime::CTimeSpanConvert::fs_CreateMinuteSpan(mp_AuthenticationLifetime);

				for (auto const &CurrentFactor : BestFactors)
				{
					TCSet<CStr> Permissions;
					for (auto const &OuterRequestedPermissions : Info.m_Request.m_RequestedPermissions)
					{
						for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
						{
							if (PermissionWithRequirements.m_Preauthenticated.f_FindEqual(CurrentFactor))
								continue; // Even if this permission requires this factor, it has been preauthenticated and should not be listed

							for (auto const &Factors : PermissionWithRequirements.m_AuthenticationFactors)
							{
								if (!Factors.f_FindEqual(CurrentFactor))
									continue;
								if (Permissions.f_FindEqual(PermissionWithRequirements.m_Permission))
									continue;

								bool bIsSubSet = true;
								for (auto const &Factor : Factors)
									bIsSubSet &= !!BestFactors.f_FindEqual(Factor) || !!PermissionWithRequirements.m_Preauthenticated.f_FindEqual(Factor);

								if (bIsSubSet)
									Permissions[PermissionWithRequirements.m_Permission];
							}
						}
					}

					auto Actor = (*_ActorInfo)[CurrentFactor].m_Actor;
					Actor
						(
						 	&ICDistributedActorTrustManagerAuthenticationActor::f_SignAuthenticationRequest
						 	, pCommandLine
						 	, Info.m_Request.m_Description
						 	, Permissions
						 	, Info.m_Challenges
						 	, fg_TempCopy(fg_Const(UserFactors)[CurrentFactor])
						 	, ExpirationTime
						)
						> AuthenticationResults.f_AddResult()
					;
				}

				AuthenticationResults.f_GetResults()
					> Continuation / [Continuation](TCVector<TCAsyncResult<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>>> &&_Results)
					{
						TCMap<CStr, TCVector<ICDistributedActorAuthenticationHandler::CResponse>> Result;
						for (auto &Responses : _Results)
						{
							if (!Responses)
							{
								Continuation.f_SetException(Responses.f_GetException());
								return;
							}
							for (auto const &Response : *Responses)
								Result[Responses->fs_GetKey(Response)].f_InsertLast(Response);
						}
						Continuation.f_SetResult(Result);
					}
				;
			}
		;

		return Continuation;
	}

	void CDistributedAppAuthenticationHandler::fp_RequestCollected(TCSharedPointer<CPreauthenticationInfo> const &_pInfo)
	{
		auto &Info = *_pInfo;
		Info.m_bAuthenticationCompleted = true;
		fp_RequestAuthentication(_pInfo) > [_pInfo](TCAsyncResult<TCMap<CStr, TCVector<CResponse>>> &&_Results)
			{
				auto &Info = *_pInfo;
				if (Info.m_bResultsSet)
					return;
				Info.m_bResultsSet = true;

				if (!_Results)
				{
					for (auto &Continuation : Info.m_Continuations)
						Continuation.f_SetException(_Results.f_GetException());
					return;
				}
				for (auto &Continuation : Info.m_Continuations)
					Continuation.f_SetResult((*_Results)[Info.m_Continuations.fs_GetKey(Continuation)]);
			}
		;
	}

	TCContinuation<TCVector<ICDistributedActorAuthenticationHandler::CResponse>> CDistributedAppAuthenticationHandler::f_RequestAuthentication
		(
			CRequest const &_Request
			, CStr const &_UserID
			, CChallenge const &_Challenge
		 	, CStr const &_MultipleRequestID
		)
	{
		if (mp_bDestroyed)
			return DMibErrorInstance("Authentication handler destroyed");

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
				return DMibErrorInstance("No such multiple request ID exists");
			pInfo = *pMultipleInfo;
			auto &Info = *pInfo;

			if (Info.m_bAuthenticationCompleted || Info.m_bResultsSet) // Ooops. Too late
				return fg_Explicit(TCVector<CResponse>{});
		}

		auto &Info = *pInfo;

		auto const &HostID = fg_GetCallingHostID();
		Info.m_Challenges[HostID] = _Challenge;
		if (!Info.m_bInitialized)
		{
			Info.m_Request = _Request;
			Info.m_UserID = _UserID;
			Info.m_bInitialized = true;
		}
		else
		{
			if (Info.m_Request != _Request)
				return DMibErrorInstance("Internal error - request mismatch between requests");
			if (Info.m_UserID != _UserID)
				return DMibErrorInstance("Internal error - UserID mismatch between requests");
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

		return Info.m_Continuations[HostID];
	}

	TCContinuation<CDistributedAppAuthenticationHandler::CMultipleRequestData> CDistributedAppAuthenticationHandler::f_GetMultipleRequestSubscription(uint32 _nHosts)
	{
		if (_nHosts == 0)
			return DMibErrorInstance("You have to have at least one host to request subscription for");
		if (mp_bDestroyed)
			return DMibErrorInstance("Authentication handler destroyed");

		auto MultipleRequestID = NCryptography::fg_RandomID();
		auto &Info = *(mp_PreauthenticationInfos[MultipleRequestID] = fg_Construct());
		Info.m_nHosts = _nHosts;

		TCActorSubscriptionWithID<> Subscription = g_ActorSubscription > [this, MultipleRequestID]() -> TCContinuation<void>
			{
				if (auto pInfo = mp_PreauthenticationInfos.f_FindEqual(MultipleRequestID))
				{
					auto &Info = **pInfo;
					if (!Info.m_bResultsSet)
					{
						Info.m_bResultsSet = true;
						for (auto &Continuation : Info.m_Continuations)
							Continuation.f_SetException(DMibErrorInstance("Multiple authentication request abandoned"));
					}
					mp_PreauthenticationInfos.f_Remove(pInfo);
				}
				return fg_Explicit();
			}
		;
		return fg_Explicit(CMultipleRequestData{fg_Move(Subscription), fg_Move(MultipleRequestID)});
	}

	TCContinuation<CActorSubscription> CDistributedAppActor::fp_SetupAuthentication(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, int64 _AuthenticationLifetime)
	{
		if (!mp_Settings.m_bSupportUserAuthentication)
			return fg_Explicit();

		return fp_EnableAuthentication(_pCommandLine, _AuthenticationLifetime);
	}

	TCContinuation<CActorSubscription> CDistributedAppActor::fp_EnableAuthentication(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, int64 _AuthenticationLifetime)
	{
		if (mp_AuthenticationHandlerImplementation)
			return DMibErrorInstance("Authentication already enabled");

		mp_AuthenticationHandlerImplementation
			= mp_State.m_DistributionManager->f_ConstructActor<CDistributedAppAuthenticationHandler>(_pCommandLine, mp_State.m_TrustManager, _AuthenticationLifetime)
		;

		auto Subscription = g_ActorSubscription > [=]() -> TCContinuation<void>
			{
				mp_AuthenticationRemotes.f_Clear();
				mp_AuthenticationRegistrationSubscriptions.f_Clear();
				mp_AuthenticationHandlerImplementation.f_Clear();

				return fg_Explicit();
			}
		;

		TCContinuation<CActorSubscription> Continuation;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser) > Continuation / [this, Continuation, Subscription = fg_Move(Subscription)](CStr &&_UserID) mutable
			{
				if (!_UserID)	// No use setting up authentication without a default user
				{
					Continuation.f_SetResult();
					return;
				}
				mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_SubscribeTrustedActors<ICDistributedActorAuthentication>
						, ICDistributedActorAuthentication::mc_pDefaultNamespace
						, fg_ThisActor(this)
					)
					> Continuation /[=, Subscription = fg_Move(Subscription)](TCTrustedActorSubscription<ICDistributedActorAuthentication> &&_Subscription) mutable
					{
						mp_AuthenticationRemotes = fg_Move(_Subscription);

						auto fOnActor = [=, UserID = fg_Move(_UserID)](TCDistributedActor<ICDistributedActorAuthentication> const &_NewActor, CTrustedActorInfo const &_ActorInfo)
							{
								TCContinuation<void> Continuation;

								mp_AuthenticationRegistrationSubscriptions[_NewActor].m_ActorInfo = _ActorInfo;

								DMibCallActor
									(
										_NewActor
										, ICDistributedActorAuthentication::f_RegisterAuthenticationHandler
										, mp_AuthenticationHandlerImplementation->f_ShareInterface<ICDistributedActorAuthenticationHandler>()
										, UserID
									) > [=, HostInfo = _ActorInfo.m_HostInfo](TCAsyncResult<TCActorSubscriptionWithID<>> &&_Subscription)
									{
										if (!_Subscription)
										{
											Continuation.f_SetException(_Subscription);
											DMibLogWithCategory
												(
													Mib/Concurrency/App
													, Error
													, "Registeration of authentication handler interface for server {} failed: {}"
													, HostInfo.f_GetDesc()
													, _Subscription.f_GetExceptionStr()
												)
											;
											return;
										}

										DMibLogWithCategory(Mib/Concurrency/App, Info, "Successfully registered authentication handler for remote {}", HostInfo.f_GetDesc());

										if (auto pSubscription = mp_AuthenticationRegistrationSubscriptions.f_FindEqual(_NewActor))
											pSubscription->m_Subscription = fg_Move(*_Subscription);

										Continuation.f_SetResult();
									}
								;

								return Continuation;
							}
						;

						mp_AuthenticationRemotes.f_OnActor
							(
								[fOnActor](TCDistributedActor<ICDistributedActorAuthentication> const &_NewActor, CTrustedActorInfo const &_ActorInfo)
								{
									fOnActor(_NewActor, _ActorInfo);
								}
								, false
							)
						;
						mp_AuthenticationRemotes.f_OnRemoveActor
							(
								[this](TCWeakDistributedActor<CActor> const &_RemovedActor)
								{
									mp_AuthenticationRegistrationSubscriptions.f_Remove(_RemovedActor);
								}
							)
						;

						TCActorResultVector<void> SubscribeResults;
						for (auto &Actor : mp_AuthenticationRemotes.m_Actors)
							fOnActor(Actor.m_Actor, Actor.m_TrustInfo) > SubscribeResults.f_AddResult();

						SubscribeResults.f_GetResults() > Continuation / [Continuation, Subscription = fg_Move(Subscription)](TCVector<TCAsyncResult<void>> &&_SubscribeResults) mutable
							{
								if (!fg_CombineResults(Continuation, fg_Move(_SubscribeResults)))
									return;

								Continuation.f_SetResult(fg_Move(Subscription));
							}
						;
					}
				;
			}
		;

		return Continuation;
	}
}

