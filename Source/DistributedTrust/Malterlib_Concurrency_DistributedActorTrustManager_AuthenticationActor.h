// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Core/RuntimeType>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationData.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	class CDistributedActorTrustManager;
	class ICDistributedActorTrustManagerAuthenticationActor;
	struct CCommandLineControl;
	
	struct CAuthenticationActorInfo
	{
		TCActor<ICDistributedActorTrustManagerAuthenticationActor> m_Actor;
		EAuthenticationFactorCategory m_Category = EAuthenticationFactorCategory_None;
	};

	class ICDistributedActorTrustManagerAuthenticationActor : public NConcurrency::CActor
	{
	public:
		ICDistributedActorTrustManagerAuthenticationActor();
		virtual ~ICDistributedActorTrustManagerAuthenticationActor();

		virtual TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) = 0;
		virtual TCContinuation<NContainer::TCMap<NStr::CStr, ICDistributedActorAuthenticationHandler::CResponse>> f_SignAuthenticationRequest
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Description
				, NContainer::TCSet<NStr::CStr> const &_Permissions
				, NContainer::TCMap<NStr::CStr, ICDistributedActorAuthenticationHandler::CChallenge> const &_Challenges
			 	, NContainer::TCMap<NStr::CStr, CAuthenticationData> &&_Factors
			 	, NTime::CTime const &_ExpirationTime
			) = 0
		;
		virtual TCContinuation<bool> f_VerifyAuthenticationResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) = 0
		;

		static NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo> fs_GetRegisteredAuthenticationFactors
			(
				TCActor<CDistributedActorTrustManager> const &_TrustManager
			)
		;
	};

	struct ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		virtual ~ICDistributedActorTrustManagerAuthenticationActorFactory() = default;
		virtual CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) = 0;
	};
}

#define	DMibAuthenticationFactorRegister(d_ClassName)												\
			DMibRuntimeClassNamedCasted																\
				(																					\
					NMib::NConcurrency::ICDistributedActorTrustManagerAuthenticationActorFactory	\
					, d_ClassName																	\
					, NMib::NConcurrency::d_ClassName												\
					, NMib::NConcurrency::ICDistributedActorTrustManagerAuthenticationActorFactory	\
				)

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
