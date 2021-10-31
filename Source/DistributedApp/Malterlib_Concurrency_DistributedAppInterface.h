// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Storage/Optional>
#include <Mib/File/DirectoryManifest>

#include "Malterlib_Concurrency_DistributedApp_Settings.h"
#include "Malterlib_Concurrency_DistributedApp_SensorReporter.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppInterfaceBackup : public CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CDistributedAppInterfaceBackup();
		~CDistributedAppInterfaceBackup();

		virtual TCFuture<void> f_AppendManifest(NFile::CDirectoryManifestConfig const &_Config) = 0;
		virtual TCFuture<TCActorSubscriptionWithID<>> f_SubscribeInitialFinished(TCActorFunctorWithID<TCFuture<void> ()> &&_fOnInitialFinished) = 0;
		virtual TCFuture<TCActorSubscriptionWithID<>> f_SubscribeBackupStopped(TCActorFunctorWithID<TCFuture<void> ()> &&_fOnStopped) = 0;
	};

	struct CDistributedAppInterfaceClient : public CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x103
		};

		CDistributedAppInterfaceClient();
		~CDistributedAppInterfaceClient();

		virtual TCFuture<void> f_GetAppStartResult() = 0;
		virtual TCFuture<void> f_PreUpdate() = 0;
		virtual TCFuture<void> f_PreStop() = 0;
		virtual TCFuture<TCActorSubscriptionWithID<>> f_StartBackup
			(
				TCDistributedActorInterfaceWithID<CDistributedAppInterfaceBackup> &&_BackupInterface
				, CActorSubscription &&_ManifestFinished
				, NStr::CStr const &_BackupRoot
			) = 0
		;
	};

	struct CDistributedAppInterfaceServer : public CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedAppInterfaceServer";

		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x104
		};

		struct CRegisterInfo
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			bool f_IsSameIgnoringLaunchID(CRegisterInfo const &_Right) const;

			EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
			NStorage::TCOptional<uint32> m_Resources_Files;
			NStorage::TCOptional<uint32> m_Resources_FilesPerProcess;
			NStorage::TCOptional<uint32> m_Resources_Threads;
			NStorage::TCOptional<uint32> m_Resources_Processes;
			NStorage::TCOptional<NStr::CStr> m_LaunchID;
		};

		CDistributedAppInterfaceServer();
		~CDistributedAppInterfaceServer();

		virtual TCFuture<TCDistributedActorInterfaceWithID<CDistributedAppSensorReporter>> f_GetSensorReporter() = 0;
		virtual TCFuture<TCActorSubscriptionWithID<>> f_RegisterDistributedApp
			(
				TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
				, TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
				, CRegisterInfo const &_RegisterInfo
			) = 0
		;
	};
}
