// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Storage/Optional>

#include "Malterlib_Concurrency_DistributedApp_Settings.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceClient : public NConcurrency::CActor
	{
		enum 
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x102
		};
		
		CDistributedAppInterfaceClient();
		~CDistributedAppInterfaceClient();

		virtual NConcurrency::TCContinuation<void> f_GetAppStartResult() = 0;
		virtual NConcurrency::TCContinuation<void> f_PreUpdate() = 0;
	};
	
	struct CDistributedAppInterfaceServer : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppInterfaceServer";
		
		enum 
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x103
		};
		
		struct CRegisterInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			
			bool operator == (CRegisterInfo const &_Right) const;
			
			EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
			NStorage::TCOptional<uint32> m_Resources_Files;
			NStorage::TCOptional<uint32> m_Resources_FilesPerProcess;
			NStorage::TCOptional<uint32> m_Resources_Threads;
			NStorage::TCOptional<uint32> m_Resources_Processes;
		};

		CDistributedAppInterfaceServer();
		~CDistributedAppInterfaceServer();
		
		virtual NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterDistributedApp
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
				, NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
				, CRegisterInfo const &_RegisterInfo
			) = 0
		;
	};
}
