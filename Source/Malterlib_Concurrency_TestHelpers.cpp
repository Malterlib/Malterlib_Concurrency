// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Network/Sockets/SSL>

#include "Malterlib_Concurrency_TestHelpers.h"

namespace NMib
{
	namespace NConcurrency
	{
		CDistributedActorTestHelper::CDistributedActorTestHelper()
			: mp_ListenSettings{1392} // 1392 is 'mib' encoded with alphabet positions
		{
			mp_ServerManager = fg_GetDistributionManager();
		}
		
		CActorDistributionCryptographySettings const &CDistributedActorTestHelper::f_GetClientCryptograhySettings() const
		{
			return mp_ClientCryptography;
		}
		
		CActorDistributionCryptographySettings const &CDistributedActorTestHelper::f_GetServerCryptograhySettings() const
		{
			return mp_ServerCryptography;
		}		
		
		void CDistributedActorTestHelper::f_SeparateServerManager()
		{
			mp_ServerManager = fg_ConstructActor<CActorDistributionManager>();
		}

		NStr::CStr CDistributedActorTestHelper::f_Init()
		{
			f_InitServer();
			return f_InitClient(*this);
		}
		
		
		void CDistributedActorTestHelper::f_InitServer()
		{
			TCActor<CActorDistributionManager> &ServerManager = mp_ServerManager;
			
			mp_ServerCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			
			mp_ListenSettings.f_SetCryptography(mp_ServerCryptography);
			mp_ListenSettings.m_bRetryOnListenFailure = false;
			ServerManager(&CActorDistributionManager::f_Listen, mp_ListenSettings).f_CallSync(60.0);

		}

		NStr::CStr CDistributedActorTestHelper::f_InitClient(CDistributedActorTestHelper &_Server)
		{
			TCActor<CActorDistributionManager> &ClientManager = mp_ClientManager; 
			
			ClientManager = fg_ConstructActor<CActorDistributionManager>();

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
			
			return NNet::CSSLContext::fs_GetCertificateFingerprint(SignedRequest);
		}
		
		void CDistributedActorTestHelper::f_Subscribe(NStr::CStr const &_Namespace)
		{
			TCActor<CActorDistributionManager> &ClientManager = mp_ClientManager;
			
			auto &ConcurrentActor = fg_ConcurrentActor();
			
			mp_RemoteEvents = 0;
			
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
						++mp_RemoteEvents;
					}
					, [&](TCWeakDistributedActor<CActor> const &_RemovedActor)
					{
						DMibLock(mp_RemoteLock);
						if (_RemovedActor == mp_RemoteActor)
							mp_RemoteActor.f_Clear();
						mp_RemoteEvent.f_Signal();
						++mp_RemoteEvents;
					}
				).f_CallSync(60.0)
			;
			
			bool bTimedOutWatingForActor = false;
			while (!bTimedOutWatingForActor)
			{
				{
					DMibLock(mp_RemoteLock);
					if (mp_RemoteEvents >= 1)
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
	}
}
