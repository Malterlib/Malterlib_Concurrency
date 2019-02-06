// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Storage/Optional>
#include <Mib/File/DirectoryManifest>

#include "Malterlib_Concurrency_DistributedApp_Settings.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceBackup : public NConcurrency::CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CDistributedAppInterfaceBackup();
		~CDistributedAppInterfaceBackup();

		virtual NConcurrency::TCFuture<void> f_AppendManifest(NFile::CDirectoryManifestConfig const &_Config) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_SubscribeInitialFinished
			(
				NConcurrency::TCActorFunctorWithID<TCFuture<void> ()> &&_fOnInitialFinished
			) = 0
		;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_SubscribeBackupStopped
			(
				NConcurrency::TCActorFunctorWithID<TCFuture<void> ()> &&_fOnStopped
			) = 0
		;
	};
	
	struct CDistributedAppInterfaceClient : public NConcurrency::CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x103
		};
		
		CDistributedAppInterfaceClient();
		~CDistributedAppInterfaceClient();

		virtual NConcurrency::TCFuture<void> f_GetAppStartResult() = 0;
		virtual NConcurrency::TCFuture<void> f_PreUpdate() = 0;
		virtual NConcurrency::TCFuture<void> f_PreStop() = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_StartBackup
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> &&_BackupInterface
				, NConcurrency::CActorSubscription &&_ManifestFinished
				, NStr::CStr const &_BackupRoot
			) = 0
		;
	};
	
	struct CDistributedAppInterfaceServer : public NConcurrency::CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppInterfaceServer";
		
		enum : uint32
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
		
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterDistributedApp
			(
				NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
				, NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
				, CRegisterInfo const &_RegisterInfo
			) = 0
		;
	};
}
