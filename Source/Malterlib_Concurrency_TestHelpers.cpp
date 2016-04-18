// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_TestHelpers.h"

namespace NMib
{
	namespace NConcurrency
	{
		CDistributedActorTestHelper::CDistributedActorTestHelper()
		{
			mp_ServerManager = fg_GetDistributionManager();
		}
		
		void CDistributedActorTestHelper::f_SeparateServerManager()
		{
			mp_ServerManager = fg_ConstructActor<CActorDistributionManager>();
		}

		void CDistributedActorTestHelper::f_Init()
		{
			TCActor<CActorDistributionManager> &ServerManager = mp_ServerManager;
			TCActor<CActorDistributionManager> &ClientManager = mp_ClientManager; 
			
			ClientManager = fg_ConstructActor<CActorDistributionManager>();
			
			CActorDistributionCryptographySettings ServerCryptography;
			ServerCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			
			CActorDistributionListenSettings ListenSettings{1392}; // 1392 is 'mib' encoded with alphabet positions
			ListenSettings.f_SetCryptography(ServerCryptography);
			ListenSettings.m_bRetryOnListenFailure = false;
			ServerManager(&CActorDistributionManager::f_Listen, ListenSettings).f_CallSync(60.0);

			CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = "wss://localhost:1392/";
			ConnectionSettings.m_PublicServerCertificate = ListenSettings.m_PublicCertificate;
			CActorDistributionCryptographySettings ClientCryptography;
			ClientCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			auto CertificateRequest = ClientCryptography.f_GenerateRequest();
			
			auto SignedRequest = ServerCryptography.f_SignRequest(CertificateRequest);
			
			ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, ServerCryptography.m_PublicCertificate, SignedRequest);
			
			ConnectionSettings.f_SetCryptography(ClientCryptography);
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0);
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
