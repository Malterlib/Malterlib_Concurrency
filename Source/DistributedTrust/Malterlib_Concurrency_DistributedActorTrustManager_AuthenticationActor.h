// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Core/RuntimeType>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	class ICDistributedActorTrustManagerAuthenticationActor : public NConcurrency::CActor
	{
	public:
		ICDistributedActorTrustManagerAuthenticationActor();
		virtual ~ICDistributedActorTrustManagerAuthenticationActor();

		virtual TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) = 0;

		static NContainer::TCMap<NStr::CStr, TCActor<ICDistributedActorTrustManagerAuthenticationActor>> fs_GetRegisteredAuthenticationFactors
			(
				TCActor<CDistributedActorTrustManager> const &_TrustManager
			)
		;
	};

	struct ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		virtual TCActor<ICDistributedActorTrustManagerAuthenticationActor> operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) = 0;
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
