// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"

#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/LogError>
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
		if (!_Handler)
			co_return DMibErrorInstance("Invalid authentication handler");

		auto &Internal = *m_pThis->mp_pInternal;
		auto CallingHostInfo = fg_GetCallingHostInfo();
		auto &UniqueHostID = CallingHostInfo.f_GetUniqueHostID();
		auto pHost = CallingHostInfo.f_GetHost();
		if (!pHost)
			co_return DMibErrorInstance("Host is not valid");

		auto pWeakHost = pHost.f_Weak();

		DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Registering authentication handler for host '{}' user '{}'", UniqueHostID, _UserID);

		NStr::CStr UserName = "(Unknown)";

		if (auto pUser = Internal.m_Users.f_FindEqual(_UserID))
			UserName = pUser->m_UserInfo.m_UserName;

		auto pWeakHandler = _Handler.f_Weak();

		co_await Internal.m_ActorDistributionManager
			(
			 	&CActorDistributionManager::f_SetAuthenticationHandler
			 	, pHost
			 	, CallingHostInfo.f_LastExecutionID()
			 	, fg_Move(_Handler)
			 	, _UserID
			 	, UserName
			)
		;

		co_return g_ActorSubscription / [pThis = m_pThis, UniqueHostID, pWeakHost, pWeakHandler]() -> TCFuture<void>
			{
				DMibLogWithCategory(Mib/Concurrency/Trust, Info, "Deregistering authentication handler for {}", UniqueHostID);
				auto pHost = pWeakHost.f_Lock();
				if (!pHost)
					co_return {};

				auto &Internal = *pThis->mp_pInternal;
				co_return co_await Internal.m_ActorDistributionManager
					(
						&CActorDistributionManager::f_RemoveAuthenticationHandler
						, fg_Move(pHost)
						, pWeakHandler
					)
				;
			}
		;
	}

	TCFuture<bool> CDistributedActorTrustManager::CInternal::CDistributedActorAuthenticationImplementation::f_AuthenticatePermissionPattern
		(
			NStr::CStr const &_Pattern
			, NContainer::TCSet<NStr::CStr> const &_AuthenticationFactors
		 	, NStr::CStr const &_RequestID
		)
	{
		auto &Internal = *m_pThis->mp_pInternal;
		return fg_CallSafe(Internal, &CDistributedActorTrustManager::CInternal::f_AuthenticatePermissionPattern, _Pattern, _AuthenticationFactors, fg_GetCallingHostInfo(), _RequestID);
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
		using namespace NStr;

		if (_Responses.f_IsEmpty())
			co_return {};

		if (!CActorDistributionManager::fs_IsValidUserID(_Challenge.m_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		auto *pRegisteredFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_Challenge.m_UserID);

		TCActorResultVector<ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn> VerificationResults;

		NContainer::TCVector<NStr::CStr> FactorIDs;

		auto fAddFalseResult = [&]
			{
				FactorIDs.f_Insert();
				TCPromise<ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn> Result;
				(Result <<= ICDistributedActorTrustManagerAuthenticationActor::CVerifyAuthenticationReturn{}) > VerificationResults.f_AddResult();
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

		auto UnwrappedResults = co_await VerificationResults.f_GetResults();

		NContainer::TCVector<bool> Results;
		auto iFactorID = FactorIDs.f_GetIterator();
		for (auto iAuthenticationResult = UnwrappedResults.f_GetIterator(); iAuthenticationResult; ++iAuthenticationResult, ++iFactorID)
		{
			auto &AuthenticationResult = *iAuthenticationResult;

			if (!AuthenticationResult)
				co_return AuthenticationResult.f_GetException();

			Results.f_InsertLast(AuthenticationResult->m_bVerified);

			if (!AuthenticationResult->m_bVerified || (AuthenticationResult->m_UpdatedPrivateData.f_IsEmpty() && AuthenticationResult->m_UpdatedPublicData.f_IsEmpty()))
				continue;

			auto *pRegisteredFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_Challenge.m_UserID);
			if (!pRegisteredFactors)
				continue;

			auto &FactorID = *iFactorID;
			auto *pRegisteredFactor = pRegisteredFactors->f_FindEqual(FactorID);
			if (!pRegisteredFactor)
				continue;

			CAuthenticationData NewData;
			NewData = pRegisteredFactor->m_AuthenticationFactor;

			for (auto &Value : AuthenticationResult->m_UpdatedPrivateData)
				NewData.m_PrivateData[AuthenticationResult->m_UpdatedPrivateData.fs_GetKey(Value)] = fg_Move(Value);
			for (auto &Value : AuthenticationResult->m_UpdatedPublicData)
				NewData.m_PublicData[AuthenticationResult->m_UpdatedPublicData.fs_GetKey(Value)] = fg_Move(Value);

			self(&CDistributedActorTrustManager::f_SetUserAuthenticationFactor, _Challenge.m_UserID, FactorID, fg_Move(NewData))
				> fg_LogError("Mib/Concurrency/Trust", "Failed to update authentication factor after verifying responses for user '{}'"_f << _Challenge.m_UserID)
			;
		}

		co_return fg_Move(Results);
	}
}
