// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	CDistributedActorTrustManager::CDistributedActorTrustManager
		(
			NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
			, COptions &&_Options
		)
		: mp_pInternal(fg_Construct(this, _Database, fg_Move(_Options)))
	{
		fp_Init();
	}
		
	CDistributedActorTrustManager::~CDistributedActorTrustManager()
	{
	}
	
	CDistributedActorTrustManager::CInternal::CInternal
		(
			CDistributedActorTrustManager *_pThis
			, NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
			, COptions &&_Options
		)
		: m_pThis(_pThis)
		, m_Database(_Database)
		, m_fDistributionManagerFactory(fg_Move(_Options.m_fConstructManager))
		, m_KeySetting(_Options.m_KeySetting)
		, m_ListenFlags(_Options.m_ListenFlags)
		, m_FriendlyName(_Options.m_FriendlyName)
		, m_Enclave(_Options.m_Enclave)
		, m_TranslateHostnames(_Options.m_TranslateHostnames)
		, m_InitialConnectionTimeout(_Options.m_InitialConnectionTimeout)
		, m_DefaultConnectionConcurrency(_Options.m_DefaultConnectionConcurrency)
		, m_bRetryOnListenFailureDuringInit(_Options.m_bRetryOnListenFailureDuringInit)
		, m_bWaitForConnectionsDuringInit(_Options.m_bWaitForConnectionsDuringInit)
		, m_bSupportAuthentication(_Options.m_bSupportAuthentication)
		, m_HostTimeout(_Options.m_HostTimeout)
		, m_HostDaemonTimeout(_Options.m_HostDaemonTimeout)
		, m_ReconnectDelay(_Options.m_ReconnectDelay)
		, m_bTimeoutForUnixSockets(_Options.m_bTimeoutForUnixSockets)
	{
	}
	
	CDistributedActorTrustManager::CInternal::~CInternal()
	{
		*m_pDestroyed = true;
	}
	
	TCFuture<void> CDistributedActorTrustManager::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		CLogError LogError("Mib/Concurrency/Trust");

		co_await Internal.m_AuthenticationInterface.f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy authentication interface");

		TCActorResultVector<void> Stops;
		for (auto &Connection : Internal.m_ClientConnections)
		{
			for (auto &ConnectionReference : Connection.m_ConnectionReferences)
				ConnectionReference.f_Disconnect() > Stops.f_AddResult();
		}
		
		for (auto &Listen : Internal.m_Listen)
			Listen.m_ListenReference.f_Stop() > Stops.f_AddResult();
		
		auto StopResults = co_await Stops.f_GetResults();

		for (auto &Result : StopResults)
		{
			if (!Result)
			{
				DMibLogWithCategory
					(
						Mib/Concurrency/Trust
						, Error
						, "Failed to stop connection or listen: {}"
						, Result.f_GetExceptionStr()
					)
				;
			}
		}

		if (Internal.m_fDistributionManagerFactory && Internal.m_ActorDistributionManager)
			co_await Internal.m_ActorDistributionManager.f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy actor distribution manager");

		co_await fg_Move(Internal.m_InitSequencer).f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy init sequencer");

		co_return {};
	}
	
	TCFuture<void> CDistributedActorTrustManager::f_RemoveClient(NStr::CStr const &_HostID)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveClient, _HostID) % "Failed to remove client from trust database");
		co_await (Internal.m_ActorDistributionManager(&CActorDistributionManager::f_KickHost, _HostID) % "Failed to kick host from distribution manager");

		co_return {};
	}

	auto CDistributedActorTrustManager::f_EnumClients() -> TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>>
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto DatabaseClients = co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClients, true) % "Failed to enum clients in database");

		NContainer::TCMap<NStr::CStr, CHostInfo> Clients;
		for (auto &ClientEntry : DatabaseClients.f_Entries())
		{
			auto &Client = Clients[ClientEntry.f_Key()];
			Client.m_HostID = ClientEntry.f_Key();
			Client.m_FriendlyName = ClientEntry.f_Value().m_LastFriendlyName;
		}

		co_return fg_Move(Clients);
	}
	
	TCFuture<bool> CDistributedActorTrustManager::f_HasClient(NStr::CStr const &_HostID)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		bool bExists = co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_HasClient, _HostID) % "Failed to check for client existence");

		co_return bExists;
	}
	
	TCFuture<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> CDistributedActorTrustManager::f_GetDistributionManager() const
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_return Internal.m_ActorDistributionManager;
	}
	
	TCFuture<NStr::CStr> CDistributedActorTrustManager::f_GetHostID() const
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_return Internal.m_BasicConfig.m_HostID;
	}
	
	CHostInfo NDistributedActorTrustManagerDatabase::CClientConnection::f_GetHostInfo() const
	{
		CHostInfo Info;
		try
		{
			Info.m_HostID = CActorDistributionManager::fs_GetCertificateHostID(m_PublicServerCertificate);
		}
		catch (NException::CException const &)
		{
		}
		Info.m_FriendlyName = m_LastFriendlyName;
		return Info;
	}
	
	int32 NDistributedActorTrustManagerDatabase::CClientConnection::f_GetEffectiveConnectionConcurrency(int32 _DefaultConcurrency) const
	{
		int32 ConnectionConcurrency = m_ConnectionConcurrency;
		if (ConnectionConcurrency == -1)
			ConnectionConcurrency = _DefaultConcurrency;
		return fg_Clamp(ConnectionConcurrency, 1, 128);
	}
	
	TCFuture<CHostInfo> CDistributedActorTrustManager::fp_GetHostInfo(NStr::CStr const &_HostID)
	{
		TCPromise<CHostInfo> Promise;

		CHostInfo HostInfo;
		HostInfo.m_HostID = _HostID;
		auto &Internal = *mp_pInternal;
		auto *pHost = Internal.m_Hosts.f_FindEqual(_HostID);
		if (pHost && !pHost->m_FriendlyName.f_IsEmpty())
		{
			HostInfo.m_FriendlyName = pHost->m_FriendlyName;
			return Promise <<= HostInfo;
		}
		for (auto &ClientConnection : Internal.m_ClientConnections)
		{
			auto ClientHostInfo = ClientConnection.m_ClientConnection.f_GetHostInfo();
			if (ClientHostInfo.m_HostID != _HostID)
				continue;
			if (ClientHostInfo.m_FriendlyName.f_IsEmpty())
				continue;
			return Promise <<= ClientHostInfo;
		}
		Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_GetClient, _HostID)
			> [Promise, HostInfo = fg_Move(HostInfo)](TCAsyncResult<ICDistributedActorTrustManagerDatabase::CClient> &&_Client) mutable
			{
				if (_Client)
					HostInfo.m_FriendlyName = _Client->m_LastFriendlyName;
				Promise.f_SetResult(fg_Move(HostInfo));
			}
		;
		return Promise.f_MoveFuture();
	}
	
	TCFuture<TCActor<ICActorDistributionManagerAccessHandler>> CDistributedActorTrustManager::f_GetAccessHandler() const
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();
		
		co_return Internal.m_AccessHandler;
	}

	CDistributedActorTrustManagerHolder::CDistributedActorTrustManagerHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
	}

	namespace NPrivate
	{
		void fg_HandleUndestroyedSubscription(TCFuture<void> &&_Result)
		{
			fg_Move(_Result) > fg_LogWarning("DistributedActorTrustManager", "Failure in trusted actor subscription destroy (subscription went out of scope)");
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
