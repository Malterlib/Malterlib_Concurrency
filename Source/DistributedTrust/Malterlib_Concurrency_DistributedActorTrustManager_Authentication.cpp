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

	TCContinuation<TCActorSubscriptionWithID<>> CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::f_RegisterAuthenticationHandler
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

		TCContinuation<TCActorSubscriptionWithID<>> Continuation;

		Internal.m_ActorDistributionManager
			(
			 	&CActorDistributionManager::f_SetAuthenticationHandler
			 	, pHost
			 	, CallingHostInfo.f_LastExecutionID()
			 	, fg_Move(_Handler)
			 	, _UserID
			)
			> Continuation / [pThis = m_pThis, Continuation, UniqueHostID, pWeakHost, pWeakHandler = _Handler.f_Weak()]
			{
				auto ActorSubscription = g_ActorSubscription > [pThis, UniqueHostID, pWeakHost, pWeakHandler]() -> TCContinuation<void>
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

				Continuation.f_SetResult(fg_Move(ActorSubscription));
			}
		;
		return Continuation;
	}

	TCContinuation<bool> CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::f_AuthenticatePermissionPattern
		(
			NStr::CStr const &_Pattern
			, NContainer::TCSet<NStr::CStr> const &_AuthenticationFactors
		 	, NStr::CStr const &_RequestID
		)
	{
		auto &Internal = *m_pThis->mp_pInternal;
		return Internal.f_AuthenticatePermissionPattern
			(
				fg_TempCopy(_Pattern)
				, fg_TempCopy(_AuthenticationFactors)
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

	TCContinuation<NContainer::TCVector<bool>> CDistributedActorTrustManager::f_VerifyAuthenticationResponses
		(
			ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
		 	, ICDistributedActorAuthenticationHandler::CRequest const &_Request
			, NContainer::TCVector<ICDistributedActorAuthenticationHandler::CResponse> const &_Responses
			, NStr::CStr const &_UserID
		) const
	{
		// TODO: Verify _Responses against _Request

		if (_Responses.f_IsEmpty())
			return fg_Explicit();

		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		auto *pRegisteredFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID);
		TCContinuation<NContainer::TCVector<bool>> Continuation;

		TCActorResultVector<bool> VerificationResults;

		auto fAddFalseResult = [&]
			{
				TCContinuation<bool> Result;
				Result.f_SetResult(false);
				Result > VerificationResults.f_AddResult();
			}
		;

		for (auto &Response : _Responses)
		{
			if (!pRegisteredFactors)
			{
				fAddFalseResult();
				continue;
			}

			auto *pRegisteredFactor = pRegisteredFactors->f_FindEqual(Response.m_FactorID);
			if (!pRegisteredFactor)
			{
				fAddFalseResult();
				continue;
			}

			auto pAuthenticationActor = Internal.m_AuthenticationActors.f_FindEqual(pRegisteredFactor->m_AuthenticationFactor.m_Name);
			if (!pAuthenticationActor)
			{
				fAddFalseResult();
				continue;
			}

			CAuthenticationData Data;
			Data = pRegisteredFactor->m_AuthenticationFactor;
			pAuthenticationActor->m_Actor(&ICDistributedActorTrustManagerAuthenticationActor::f_VerifyAuthenticationResponse, Response, _Challenge, Data)
				> VerificationResults.f_AddResult()
			;
		}

		VerificationResults.f_GetResults() > Continuation / [Continuation](TCVector<TCAsyncResult<bool>> &&_Results)
			{
				NContainer::TCVector<bool> Results;
				for (auto &AuthenticationResult : _Results)
				{
					if (!AuthenticationResult)
						return Continuation.f_SetException(AuthenticationResult.f_GetException());

					Results.f_InsertLast(*AuthenticationResult);
				}
				Continuation.f_SetResult(Results);
			}
		;
		return Continuation;
	}
}


