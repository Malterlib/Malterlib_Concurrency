// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Cryptography/RandomData>

namespace NMib::NConcurrency
{
	using namespace NContainer;
	
	CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::CDistributedActorAuthenticationImplementation()
	{
		DMibPublishActorFunction(ICDistributedActorAuthentication::f_RegisterAuthenticationHandler);
		DMibPublishActorFunction(ICDistributedActorAuthentication::f_AuthenticatePermissionPattern);
	}

	TCFuture<TCActorSubscriptionWithID<>> CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::f_RegisterAuthenticationHandler
		(
			TCDistributedActorInterfaceWithID<ICDistributedActorAuthenticationHandler> &&_Handler
			, NStr::CStr const &_UserID
		)
	{
		auto &Internal = *m_pThis->mp_pInternal;
		auto &CallingHostInfo = fg_GetCallingHostInfo();
		auto &UniqueHostID = CallingHostInfo.f_GetUniqueHostID();
		auto pHost = CallingHostInfo.f_GetHost();
		if (!pHost)
			return DMibErrorInstance("Host is not valid");

		auto pWeakHost = pHost.f_Weak();

		DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Registering authentication handler for host '{}' user '{}'", UniqueHostID, _UserID);

		TCPromise<TCActorSubscriptionWithID<>> Promise;

		NStr::CStr UserName = "(Unknown)";

		if (auto pUser = Internal.m_Users.f_FindEqual(_UserID))
			UserName = pUser->m_UserInfo.m_UserName;

		Internal.m_ActorDistributionManager
			(
			 	&CActorDistributionManager::f_SetAuthenticationHandler
			 	, pHost
			 	, CallingHostInfo.f_LastExecutionID()
			 	, fg_Move(_Handler)
			 	, _UserID
			 	, UserName
			)
			> Promise / [pThis = m_pThis, Promise, UniqueHostID, pWeakHost, pWeakHandler = _Handler.f_Weak()]
			{
				auto ActorSubscription = g_ActorSubscription / [pThis, UniqueHostID, pWeakHost, pWeakHandler]() -> TCFuture<void>
					{
						DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Deregistering authentication handler for {}", UniqueHostID);
						auto pHost = pWeakHost.f_Lock();
						if (!pHost)
							return fg_Explicit();

						auto &Internal = *pThis->mp_pInternal;
						return Internal.m_ActorDistributionManager
							(
							 	&CActorDistributionManager::f_RemoveAuthenticationHandler
							 	, pHost
							 	, pWeakHandler
							)
						;
					}
				;

				Promise.f_SetResult(fg_Move(ActorSubscription));
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<bool> CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::f_AuthenticatePermissionPattern
		(
			NStr::CStr const &_Pattern
			, NContainer::TCSet<NStr::CStr> const &_AuthenticationFactors
		 	, NStr::CStr const &_RequestID
		)
	{
		auto &Internal = *m_pThis->mp_pInternal;
		return Internal.f_AuthenticatePermissionPattern
			(
				_Pattern
				, _AuthenticationFactors
				, fg_GetCallingHostInfo()
			 	, _RequestID
			)
		;
	}

	ICDistributedActorAuthenticationHandler::CChallenge CDistributedActorTrustManager::fs_GenerateAuthenticationChallenge(NStr::CStr const &_UserID)
	{
		ICDistributedActorAuthenticationHandler::CChallenge Challenge;
		NCryptography::fg_GenerateRandomData(Challenge.m_ChallengeData.f_GetArray(32), 32);
		Challenge.m_UserID = _UserID;
		return Challenge;
	}

	TCFuture<NContainer::TCVector<bool>> CDistributedActorTrustManager::f_VerifyAuthenticationResponses
		(
			ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
		 	, ICDistributedActorAuthenticationHandler::CRequest const &_Request
			, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CResponse> const &_Responses
		)
	{
		if (_Responses.f_IsEmpty())
			return fg_Explicit();

		if (!CActorDistributionManager::fs_IsValidUserID(_Challenge.m_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		auto *pRegisteredFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_Challenge.m_UserID);
		TCPromise<NContainer::TCVector<bool>> Promise;

		TCActorResultVector<ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn> VerificationResults;

		NContainer::TCVector<NStr::CStr> FactorIDs;

		auto fAddFalseResult = [&]
			{
				FactorIDs.f_Insert();
				TCPromise<ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn> Result;
				Result.f_SetResult(ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn{});
				Result > VerificationResults.f_AddResult();
			}
		;

		for (auto &Response : _Responses)
		{
			if (!pRegisteredFactors)
			{
				DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Verify authentication response failed: Factor '{}' not found for user '{}'", Response.m_FactorID, _Challenge.m_UserID);
				fAddFalseResult();
				continue;
			}

			auto *pRegisteredFactor = pRegisteredFactors->f_FindEqual(Response.m_FactorID);
			if (!pRegisteredFactor)
			{
				DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Verify authentication response failed: Factor '{}' not found for user '{}'", Response.m_FactorID, _Challenge.m_UserID);
				fAddFalseResult();
				continue;
			}

			auto pAuthenticationActor = Internal.m_AuthenticationActors.f_FindEqual(pRegisteredFactor->m_AuthenticationFactor.m_Name);
			if (!pAuthenticationActor)
			{
				DMibLogWithCategory
					(
					 	Mib/Concurrency/Trust
					 	, Info
					 	, "Verify authentication response failed for user '{}': Factor of type '{}' not supported"
					 	, _Challenge.m_UserID
					 	,  pRegisteredFactor->m_AuthenticationFactor.m_Name
					)
				;
				fAddFalseResult();
				continue;
			}

			auto *pThisChallenge = Response.m_SignedProperties.m_Challenges.f_FindEqual(Internal.m_BasicConfig.m_HostID);
			if (!pThisChallenge || *pThisChallenge != _Challenge)
			{
				if (!pThisChallenge)
					DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Verify authentication response failed for user '{}': Challenge for this host not found", _Challenge.m_UserID);
				else
					DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Verify authentication response failed for user '{}': Challenge does not match signed challenge", _Challenge.m_UserID);
				fAddFalseResult();
				continue;
			}

			FactorIDs.f_Insert(Response.m_FactorID);

			CAuthenticationData Data;
			Data = pRegisteredFactor->m_AuthenticationFactor;
			pAuthenticationActor->m_Actor(&ICDistributedActorTrustManagerAuthenticationActor::f_VerifyAuthenticationResponse, Response, _Challenge, Data)
				> VerificationResults.f_AddResult()
			;
		}

		VerificationResults.f_GetResults()
			> Promise / [this, Promise, _Challenge, FactorIDs](TCVector<TCAsyncResult<ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn>> &&_Results)
			{
				auto &Internal = *mp_pInternal;
				NContainer::TCVector<bool> Results;
				auto iFactorID = FactorIDs.f_GetIterator();
				for (auto &AuthenticationResult : _Results)
				{
					if (!AuthenticationResult)
						return Promise.f_SetException(AuthenticationResult.f_GetException());

					if (AuthenticationResult->m_bVerified && (!AuthenticationResult->m_UpdatedPrivateData.f_IsEmpty() ||  !AuthenticationResult->m_UpdatedPublicData.f_IsEmpty()))
					{
						auto *pRegisteredFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_Challenge.m_UserID);
						if (pRegisteredFactors)
						{
							auto &FactorID = *iFactorID;
							auto *pRegisteredFactor = pRegisteredFactors->f_FindEqual(FactorID);
							if (pRegisteredFactor)
							{
								CAuthenticationData NewData;
								NewData = pRegisteredFactor->m_AuthenticationFactor;

								for (auto &Value : AuthenticationResult->m_UpdatedPrivateData)
									NewData.m_PrivateData[AuthenticationResult->m_UpdatedPrivateData.fs_GetKey(Value)] = fg_Move(Value);
								for (auto &Value : AuthenticationResult->m_UpdatedPublicData)
									NewData.m_PublicData[AuthenticationResult->m_UpdatedPublicData.fs_GetKey(Value)] = fg_Move(Value);

								f_SetUserAuthenticationFactor(_Challenge.m_UserID, FactorID, fg_Move(NewData)) > [UserID = _Challenge.m_UserID](TCAsyncResult<void> &&_Result)
									{
										DMibLogWithCategory
											(
											 	Mib/Concurrency/Trust
											 	, Error
											 	, "Failed to update authentication factor after verifying responses for user '{}': {}"
											 	, UserID
											 	, _Result.f_GetExceptionStr()
											)
										;
									}
								;
							}
						}
					}

					Results.f_InsertLast(AuthenticationResult->m_bVerified);
					++iFactorID;
				}
				Promise.f_SetResult(Results);
			}
		;
		return Promise.f_MoveFuture();
	}
}
