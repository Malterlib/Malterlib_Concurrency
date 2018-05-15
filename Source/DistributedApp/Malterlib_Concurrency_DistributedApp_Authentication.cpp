// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include <Mib/Concurrency/DistributedApp> // For CCommandLineControl

#include <Mib/Concurrency/DistributedActorAuthenticationHandler>

namespace NMib::NConcurrency
{
	using namespace NPtr;
	using namespace NStr;
	using namespace NContainer;

	struct CDistributedAppAuthenticationHandler : public ICDistributedActorAuthenticationHandler
	{
		CDistributedAppAuthenticationHandler(TCSharedPointer<CCommandLineControl> const &_pCommandLine, TCActor<CDistributedActorTrustManager> const &_TrustManager);

		TCContinuation<TCVector<CResponse>> f_AuthenticateCommand
			(
				CRequest const &_Request
				, CStr const &_UserID
				, CChallenge const &_Challenge
			) override
		;

	private:
		NPtr::TCSharedPointer<CCommandLineControl> const mp_pCommandLine;
		TCActor<CDistributedActorTrustManager> mp_TrustManager;
	};

	CDistributedAppAuthenticationHandler::CDistributedAppAuthenticationHandler
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
		: mp_pCommandLine(_pCommandLine)
		, mp_TrustManager(_TrustManager)
	{
	}

	TCContinuation<TCVector<ICDistributedActorAuthenticationHandler::CResponse>> CDistributedAppAuthenticationHandler::f_AuthenticateCommand
		(
			CRequest const &_Request
			, CStr const &_UserID
			, CChallenge const &_Challenge
		)
	{
		TCContinuation<TCVector<CResponse>> Continuation;
		mp_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			+ mp_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _UserID)
			> [Continuation, pCommandLine = mp_pCommandLine, _Request, _Challenge]
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
				TCMap<CStr, TCMap<CStr, CAuthenticationData>> UserFactors;
				for (auto const &Data : *_RegisteredFactors)
					UserFactors[Data.m_Name][_RegisteredFactors->fs_GetKey(Data)] = Data;

				// The _Request contains a vector of vectors of permissions that needs authentication and the Factors to authenticate.
				//
				// We will try to minimize the number of actions required by the user by finding a minimal set of authentication Factors that will authenticate all cases in _Request.
				TCMap<TCSet<CStr>, TCSet<mint>> FactorCounts;
				mint nPermissionCount = 0;
				for (auto const &OuterRequestedPermissions : _Request.m_RequestedPermissions)
				{
					++nPermissionCount;
					for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
					{
						bool bCanHandleAtLeastOne = false;
						for (auto const &Factors : PermissionWithRequirements.m_AuthenticationFactors)
						{
							bool bCanHandleAll = true;

							for (auto Factor : Factors)
							{
								if (!_ActorInfo->f_FindEqual(Factor) || !UserFactors.f_FindEqual(Factor))
								{
									bCanHandleAll = false;
									break;
								}
							}
							if (bCanHandleAll)
							{
								FactorCounts[Factors][nPermissionCount];
								bCanHandleAtLeastOne = true;
							}
						}
						if (!bCanHandleAtLeastOne)
							return Continuation.f_SetResult(TCVector<CResponse>{});	// This permission will never be authenticated
					}
				}

				// Iterate through all collected Factor sets add the members for subsets, they are handled for free if we choose the superset
				for (auto &Members : FactorCounts)
				{
					auto const &Factors = FactorCounts.fs_GetKey(Members);
					if (Factors.f_GetLen() > 1)
					{
						// Here we should iterate through the full powerset. As long as we limit ourselves to TWO factor authentication it is enough to go through the individual
						// members of the set, but once we start using more than two factors, say (A, B, C) we should look for (A, B), (A,C), and (B, C) as well.
						for (auto const &Factor : Factors)
						{
							if (auto *pSubSetMembers = FactorCounts.f_FindEqual(TCSet<CStr>{Factor}))
								Members += *pSubSetMembers;
						}
					}
				}

				TCSet<CStr> BestFactors;
				TCSet<CStr> AllFactors;
				auto fIsPrefered = [_ActorInfo, &BestFactors](TCSet<CStr> const &_Factors) -> bool
					{
						// We will only come here when the two sets have the same cardinality. Select the on with the lowest category. (I.e. Try to avoid asking for password).

						EAuthenticationFactorCategory Worst1 = EAuthenticationFactorCategory_Knowledge;
						EAuthenticationFactorCategory Worst2 = EAuthenticationFactorCategory_Knowledge;
						for (auto const &Factor : BestFactors)
							Worst1 = fg_Min(Worst1, (*_ActorInfo)[Factor].m_Category);
						for (auto const &Factor : _Factors)
							Worst2 = fg_Min(Worst2, (*_ActorInfo)[Factor].m_Category);

						return Worst1 > Worst2;
					}
				;
				for (auto const &Members : FactorCounts)
				{
					auto const &Factors = FactorCounts.fs_GetKey(Members);
					AllFactors += Factors;
					if (Members.f_GetLen() == nPermissionCount)
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

				TCActorResultVector<CResponse> AuthenticationResults;

				for (auto const &CurrentFactor : BestFactors)
				{
					TCSet<CStr> Permissions;

					for (auto const &OuterRequestedPermissions : _Request.m_RequestedPermissions)
					{
						for (auto const &PermissionWithRequirements : OuterRequestedPermissions)
						{
							for (auto const &Factors : PermissionWithRequirements.m_AuthenticationFactors)
							{
								if (!Factors.f_FindEqual(CurrentFactor))
									continue;
								if (Permissions.f_FindEqual(PermissionWithRequirements.m_Permission))
									continue;

								bool bIsSubSet = true;
								for (auto const &Factor : Factors)
									bIsSubSet &= !!BestFactors.f_FindEqual(Factor);

								if (bIsSubSet)
									Permissions[PermissionWithRequirements.m_Permission];
							}
						}
					}


					auto Actor = (*_ActorInfo)[CurrentFactor].m_Actor;
					Actor
						(
						 	&ICDistributedActorTrustManagerAuthenticationActor::f_AuthenticateCommand
						 	, pCommandLine
						 	, _Request.m_Description
						 	, Permissions
						 	, _Challenge
						 	, fg_TempCopy(fg_Const(UserFactors)[CurrentFactor])
						)
						> AuthenticationResults.f_AddResult()
					;
				}

				AuthenticationResults.f_GetResults() > Continuation / [Continuation](TCVector<TCAsyncResult<CResponse>> &&_Results)
					{
						TCVector<CResponse> CombinedResults;

						for (auto &Result : _Results)
						{
							if (!Result)
							{
								Continuation.f_SetException(Result.f_GetException());
								return;
							}
							CombinedResults.f_InsertLast(*Result);
						}
						Continuation.f_SetResult(CombinedResults);
					}
				;
			}
		;

		return Continuation;
	}

	TCContinuation<void> CDistributedAppActor::fp_SetupAuthentication(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		if (!mp_Settings.m_bSupportUserAuthentication)
			return fg_Explicit();

		return fp_EnableAuthentication(_pCommandLine);
	}

	TCContinuation<void> CDistributedAppActor::fp_EnableAuthentication(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		mp_AuthenticationHandlerImplementation = mp_State.m_DistributionManager->f_ConstructActor<CDistributedAppAuthenticationHandler>(_pCommandLine, mp_State.m_TrustManager);

		TCContinuation<void> Continuation;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser) > Continuation / [this, Continuation](CStr &&_UserID)
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
					> Continuation /[=](TCTrustedActorSubscription<ICDistributedActorAuthentication> &&_Subscription) mutable
					{
						mp_AuthenticationRemotes = fg_Move(_Subscription);

						auto fOnActor = [=, UserID = fg_Move(_UserID)](TCDistributedActor<ICDistributedActorAuthentication> const &_NewActor, CTrustedActorInfo const &_ActorInfo)
							{
								TCContinuation<void> Continuation;

								mp_AuthenticationRegistrationSubscriptions[_NewActor];

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
											*pSubscription = fg_Move(*_Subscription);

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

						SubscribeResults.f_GetResults() > Continuation / [Continuation](TCVector<TCAsyncResult<void>> &&_SubscribeResults)
							{
								if (!fg_CombineResults(Continuation, fg_Move(_SubscribeResults)))
									return;

								Continuation.f_SetResult();
							}
						;
					}
				;
			}
		;

		return Continuation;
	}
}

