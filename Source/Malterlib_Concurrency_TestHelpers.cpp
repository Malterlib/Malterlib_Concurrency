// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Network/Sockets/SSL>

#include "Malterlib_Concurrency_TestHelpers.h"
#include <Mib/Cryptography/RandomID>
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

namespace NMib
{
	namespace NConcurrency
	{
		CDistributedActorTestHelperCombined::CDistributedActorTestHelperCombined()
			: mp_ListenSettings{1392} // 1392 is 'mib' encoded with alphabet positions
			, mp_ServerCryptography(NCryptography::fg_RandomID())
			, mp_ClientCryptography(NCryptography::fg_RandomID())
		{
		}

		CDistributedActorTestHelperCombined::~CDistributedActorTestHelperCombined()
		{
		}

		CActorDistributionCryptographySettings const &CDistributedActorTestHelperCombined::f_GetClientCryptograhySettings() const
		{
			return mp_ClientCryptography;
		}
		
		CActorDistributionCryptographySettings const &CDistributedActorTestHelperCombined::f_GetServerCryptograhySettings() const
		{
			return mp_ServerCryptography;
		}		
		
		NStr::CStr const &CDistributedActorTestHelperCombined::f_GetServerHostID() const
		{
			return mp_pServer->f_GetHostID(); 
		}
		
		NStr::CStr const &CDistributedActorTestHelperCombined::f_GetClientHostID() const
		{
			return mp_pClient->f_GetHostID(); 
		}
		
		void CDistributedActorTestHelperCombined::f_SeparateServerManager()
		{
			mp_pServer = fg_Construct(mp_ServerCryptography.m_HostID, fg_ConstructActor<CActorDistributionManager>(mp_ServerCryptography.m_HostID));
		}

		void CDistributedActorTestHelperCombined::f_Init()
		{
			f_InitServer();
			f_InitClient(*this);
		}
		
		
		void CDistributedActorTestHelperCombined::f_InitServer()
		{
			if (!mp_pServer)
			{
				mp_ServerCryptography.m_HostID = fg_InitDistributionManager(mp_ServerCryptography.m_HostID);
				mp_pServer = fg_Construct(mp_ServerCryptography.m_HostID, fg_GetDistributionManager());
			}
			
			TCActor<CActorDistributionManager> const &ServerManager = mp_pServer->f_GetManager();
			
			mp_ServerCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			
			mp_ListenSettings.f_SetCryptography(mp_ServerCryptography);
			mp_ListenSettings.m_bRetryOnListenFailure = false;
			ServerManager(&CActorDistributionManager::f_Listen, mp_ListenSettings).f_CallSync(60.0);
		}

		void CDistributedActorTestHelperCombined::f_InitClient(CDistributedActorTestHelperCombined &_Server)
		{
			mp_pClient = fg_Construct(mp_ClientCryptography.m_HostID, fg_ConstructActor<CActorDistributionManager>(mp_ClientCryptography.m_HostID));
			
			TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager(); 
			
			CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = "wss://localhost:1392/";
			ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_PublicCertificate;
			mp_ClientCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			auto CertificateRequest = mp_ClientCryptography.f_GenerateRequest();
			
			auto SignedRequest = _Server.mp_ServerCryptography.f_SignRequest(CertificateRequest);
			
			mp_ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, _Server.mp_ServerCryptography.m_PublicCertificate, SignedRequest);
			
			ConnectionSettings.f_SetCryptography(mp_ClientCryptography);
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0);
		}
		
		void CDistributedActorTestHelperCombined::f_Subscribe(NStr::CStr const &_Namespace)
		{
			mp_pClient->f_Subscribe(_Namespace);
		}
		
		void CDistributedActorTestHelperCombined::f_Unsubscribe()
		{
			mp_pClient->f_Unsubscribe();
		}			
		
		void CDistributedActorTestHelperCombined::f_Unpublish()
		{
			mp_pServer->f_Unpublish();
		}
		
		TCActor<CActorDistributionManager> const &CDistributedActorTestHelper::f_GetManager() const
		{
			return mp_Manager;			
		}

		CDistributedActorTestHelper::CDistributedActorTestHelper(NStr::CStr const &_HostID, TCActor<CActorDistributionManager> const &_Manager)
			: mp_HostID(_HostID)
			, mp_Manager(_Manager) 
		{
		}
		
		CDistributedActorTestHelper::CDistributedActorTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager)
			: mp_HostID(_TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0))
			, mp_Manager(_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(60.0)) 
		{
			auto State = _TrustManager(&CDistributedActorTrustManager::f_GetConnectionState, true).f_CallSync(60.0);
			
			for (auto &Host : State.m_Hosts)
			{
				for (auto &Address : Host.m_Addresses)
				{
					if (Address.m_bConnected)
						DMibError("Found unconnected address");
				}			
			}
		}
		
		CDistributedActorTestHelper::~CDistributedActorTestHelper()
		{
		}


		void CDistributedActorTestHelper::f_Subscribe(NStr::CStr const &_Namespace)
		{
			TCActor<CActorDistributionManager> &ClientManager = mp_Manager;
			
			auto &ConcurrentActor = fg_ConcurrentActor();
			
			mp_RemoteActorsSubscription = ClientManager
				(
					&CActorDistributionManager::f_SubscribeActors
					, NContainer::fg_CreateVector<NStr::CStr>(_Namespace)
					, ConcurrentActor 
					, [&](CAbstractDistributedActor &&_NewActor)
					{
						DMibLock(mp_RemoteLock);
						mp_RemoteActor = _NewActor.f_GetActorUnsafe<CActor>();
						mp_RemoteEvent.f_Signal();
					}
					, [&](TCWeakDistributedActor<CActor> const &_RemovedActor)
					{
						DMibLock(mp_RemoteLock);
						if (_RemovedActor == mp_RemoteActor)
							mp_RemoteActor.f_Clear();
						mp_RemoteEvent.f_Signal();
					}
				).f_CallSync(60.0)
			;
			
			bool bTimedOutWatingForActor = false;
			while (!bTimedOutWatingForActor)
			{
				{
					DMibLock(mp_RemoteLock);
					if (!mp_RemoteActor.f_IsEmpty())
						break;
				}
				bTimedOutWatingForActor = mp_RemoteEvent.f_WaitTimeout(60.0);
			}
			if (bTimedOutWatingForActor)
				DMibError("bTimedOutWatingForActor");
		}
		
		void CDistributedActorTestHelper::f_Unsubscribe()
		{
			mp_RemoteActorsSubscription.f_Clear();
		}			
		
		void CDistributedActorTestHelper::f_Unpublish()
		{
			mp_Publication.f_Clear();
		}
		
		NStr::CStr const &CDistributedActorTestHelper::f_GetHostID() const
		{
			return mp_HostID; 
		}
	}
}
