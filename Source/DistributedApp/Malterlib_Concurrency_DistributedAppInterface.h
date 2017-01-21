// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceClient : public NConcurrency::CActor
	{
		CDistributedAppInterfaceClient();
		~CDistributedAppInterfaceClient();
		
		enum 
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};
		
		virtual NConcurrency::TCContinuation<void> f_PreUpdate() = 0;
	};
	
	struct CDistributedAppInterfaceServer : public NConcurrency::CActor
	{
		CDistributedAppInterfaceServer();
		~CDistributedAppInterfaceServer();
		
		enum 
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};
		
		virtual NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterDistributedApp
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
				, EDistributedAppUpdateType _UpdateType
			) = 0
		;
	};
}
