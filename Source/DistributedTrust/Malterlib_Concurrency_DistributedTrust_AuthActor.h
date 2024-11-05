// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Core/RuntimeType>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include "Malterlib_Concurrency_DistributedTrust_AuthData.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthActor.h"

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
		struct CVerifyAuthenticationReturn
		{
			bool m_bVerified = false;
			NContainer::TCMap<NStr::CStr, NEncoding::CEJSONSorted> m_UpdatedPublicData;
			NContainer::TCMap<NStr::CStr, NEncoding::CEJSONSorted> m_UpdatedPrivateData;
		};

		ICDistributedActorTrustManagerAuthenticationActor();
		virtual ~ICDistributedActorTrustManagerAuthenticationActor();

		virtual TCFuture<CAuthenticationData> f_RegisterFactor(NStr::CStr _UserID, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine) = 0;
		virtual TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine
				, NStr::CStr _Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties _SignedProperties
				, NContainer::TCMap<NStr::CStr, CAuthenticationData> _Factors
			) = 0
		;
		virtual TCFuture<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
				ICDistributedActorAuthenticationHandler::CResponse _Response
				, ICDistributedActorAuthenticationHandler::CChallenge _Challenge
				, CAuthenticationData _AuthenticationData
			) = 0
		;

		static NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo> fs_GetRegisteredAuthenticationFactors(TCActor<CDistributedActorTrustManager> _TrustManager);
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
