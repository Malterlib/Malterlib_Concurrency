// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Network/SSL>

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
	{
	}
	
	CDistributedActorTrustManager::CInternal::~CInternal()
	{
		*m_pDestroyed = true;
	}
	
	TCFuture<void> CDistributedActorTrustManager::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;
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

		co_await Internal.m_AuthenticationInterface.f_Destroy();

		if (Internal.m_fDistributionManagerFactory && Internal.m_ActorDistributionManager)
			co_await Internal.m_ActorDistributionManager->f_Destroy();

		co_return {};
	}
	
	bool CDistributedActorTrustManager_Address::operator == (CDistributedActorTrustManager_Address const &_Right) const
	{
		return m_URL == _Right.m_URL; 
	}

	bool CDistributedActorTrustManager_Address::operator < (CDistributedActorTrustManager_Address const &_Right) const
	{
		return m_URL < _Right.m_URL; 
	}
	
	TCFuture<void> CDistributedActorTrustManager::f_RemoveClient(NStr::CStr const &_HostID)
	{
		auto &Internal = *mp_pInternal;
		TCPromise<void> Promise;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _HostID]
				{
					auto &Internal = *mp_pInternal;
					Internal.m_Database
						(
							&ICDistributedActorTrustManagerDatabase::f_RemoveClient
							, _HostID
						)
						> Promise % "Failed to remove client from trust database" / [this, Promise, _HostID]()
						{
							auto &Internal = *mp_pInternal;
							Internal.m_ActorDistributionManager(&CActorDistributionManager::f_KickHost, _HostID) > Promise % "Failed to kick host from distribution manager";
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	auto CDistributedActorTrustManager::f_EnumClients() -> TCFuture<NContainer::TCMap<NStr::CStr, CHostInfo>>
	{
		auto &Internal = *mp_pInternal;
		TCPromise<NContainer::TCMap<NStr::CStr, CHostInfo>> Promise;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise]
				{
					auto &Internal = *mp_pInternal;
					Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClients, true) 
						> (Promise % "Failed to enum clients in database") / [Promise](NContainer::TCMap<NStr::CStr, CClient> &&_Info)
						{
							NContainer::TCMap<NStr::CStr, CHostInfo> Clients;
							for (auto iClient = _Info.f_GetIterator(); iClient; ++iClient)
							{
								auto &Client = Clients[iClient.f_GetKey()];
								Client.m_HostID = iClient.f_GetKey();
								Client.m_FriendlyName = iClient->m_LastFriendlyName;
							}
							Promise.f_SetResult(fg_Move(Clients));
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}
	
	TCFuture<bool> CDistributedActorTrustManager::f_HasClient(NStr::CStr const &_HostID)
	{
		auto &Internal = *mp_pInternal;
		TCPromise<bool> Promise;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise, _HostID]
				{
					auto &Internal = *mp_pInternal;
					Internal.m_Database
						(
							&ICDistributedActorTrustManagerDatabase::f_HasClient
							, _HostID
						)
						> Promise % "Failed to check for client existence" / [Promise, _HostID](bool _bExists)
						{
							Promise.f_SetResult(_bExists);
						}
					;
				}
			)
		;
		return Promise.f_MoveFuture();
	}
	
	TCFuture<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> CDistributedActorTrustManager::f_GetDistributionManager() const
	{
		auto &Internal = *mp_pInternal;
		TCPromise<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> Promise;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise]
				{
					auto &Internal = *mp_pInternal;
					Promise.f_SetResult(Internal.m_ActorDistributionManager);
				}
			)
		;
		return Promise.f_MoveFuture();
	}
	
	TCFuture<NStr::CStr> CDistributedActorTrustManager::f_GetHostID() const
	{
		auto &Internal = *mp_pInternal;
		TCPromise<NStr::CStr> Promise;
		Internal.f_RunAfterInit
			(
				Promise
				, [this, Promise]
				{
					auto &Internal = *mp_pInternal;
					Promise.f_SetResult(Internal.m_BasicConfig.m_HostID);
				}
			)
		;
		return Promise.f_MoveFuture();
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
		CHostInfo HostInfo;
		HostInfo.m_HostID = _HostID;
		auto &Internal = *mp_pInternal;
		auto *pHost = Internal.m_Hosts.f_FindEqual(_HostID);
		if (pHost && !pHost->m_FriendlyName.f_IsEmpty())
		{
			HostInfo.m_FriendlyName = pHost->m_FriendlyName;
			return fg_Explicit(HostInfo);
		}
		for (auto &ClientConnection : Internal.m_ClientConnections)
		{
			auto ClientHostInfo = ClientConnection.m_ClientConnection.f_GetHostInfo();
			if (ClientHostInfo.m_HostID != _HostID)
				continue;
			if (ClientHostInfo.m_FriendlyName.f_IsEmpty())
				continue;
			return fg_Explicit(ClientHostInfo);
		}
		TCPromise<CHostInfo> Promise;
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
	
	TCActor<ICActorDistributionManagerAccessHandler> CDistributedActorTrustManager::f_GetAccessHandler() const
	{
		auto &Internal = *mp_pInternal;
		
		return Internal.m_AccessHandler;
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
