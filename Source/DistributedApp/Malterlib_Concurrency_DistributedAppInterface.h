// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActorTrustManager>

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceClient : public NConcurrency::CActor
	{
		enum 
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};
		
		CDistributedAppInterfaceClient();
		~CDistributedAppInterfaceClient();
		
		virtual NConcurrency::TCContinuation<void> f_PreUpdate() = 0;
	};
	
	struct CDistributedAppInterfaceServer : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppInterfaceServer";
		
		enum 
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x102
		};

		CDistributedAppInterfaceServer();
		~CDistributedAppInterfaceServer();
		
		virtual NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterDistributedApp
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
				, NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
				, EDistributedAppUpdateType _UpdateType
			) = 0
		;
	};
}
