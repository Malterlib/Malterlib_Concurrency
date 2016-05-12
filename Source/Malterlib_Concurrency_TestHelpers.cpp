// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Network/Sockets/SSL>

#include "Malterlib_Concurrency_TestHelpers.h"
#include <Mib/Cryptography/RandomID>
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include <Mib/Test/Test>

namespace NMib
{
	namespace NConcurrency
	{
		CDistributedActorTestHelperCombined::CDistributedActorTestHelperCombined()
			: mp_ListenSettings{31392} // 1392 is 'mib' encoded with alphabet positions
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
			mp_ListenReference = ServerManager(&CActorDistributionManager::f_Listen, mp_ListenSettings).f_CallSync(60.0);
		}

		void CDistributedActorTestHelperCombined::f_InitClient(CDistributedActorTestHelperCombined &_Server)
		{
			mp_pClient = fg_Construct(mp_ClientCryptography.m_HostID, fg_ConstructActor<CActorDistributionManager>(mp_ClientCryptography.m_HostID));
			
			TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager(); 
			
			CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = "wss://localhost:31392/";
			ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
			mp_ClientCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), 1024);
			auto CertificateRequest = mp_ClientCryptography.f_GenerateRequest();
			
			auto SignedRequest = _Server.mp_ServerCryptography.f_SignRequest(CertificateRequest);
			
			mp_ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, _Server.mp_ServerCryptography.m_PublicCertificate, SignedRequest);
			
			ConnectionSettings.f_SetCryptography(mp_ClientCryptography);
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0).m_ConnectionReference;
		}
		
		void CDistributedActorTestHelperCombined::f_InitAnonymousClient(CDistributedActorTestHelperCombined &_Server)
		{
			mp_pClient = fg_Construct(mp_ClientCryptography.m_HostID, fg_ConstructActor<CActorDistributionManager>(mp_ClientCryptography.m_HostID));
			
			TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager(); 
			
			CActorDistributionConnectionSettings ConnectionSettings;
			ConnectionSettings.m_ServerURL = "wss://localhost:31392/";
			ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
			ConnectionSettings.m_bRetryConnectOnFailure = false;

			mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings).f_CallSync(60.0).m_ConnectionReference;
		}
		
		NStr::CStr CDistributedActorTestHelperCombined::f_Subscribe(NStr::CStr const &_Namespace)
		{
			return mp_pClient->f_Subscribe(_Namespace);
		}
		
		void CDistributedActorTestHelperCombined::f_Unsubscribe(NStr::CStr const &_Namespace)
		{
			mp_pClient->f_Unsubscribe(_Namespace);
		}			
		
		void CDistributedActorTestHelperCombined::f_Unpublish(NStr::CStr const &_Publication)
		{
			mp_pServer->f_Unpublish(_Publication);
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
		
		CDistributedActorTestHelper::CDistributedActorTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager, bool _bAllowUnconnected)
			: mp_HostID(_TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0))
			, mp_Manager(_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(60.0)) 
		{
			if (!_bAllowUnconnected)
			{
				auto State = _TrustManager(&CDistributedActorTrustManager::f_GetConnectionState, true).f_CallSync(60.0);
				for (auto &Host : State.m_Hosts)
				{
					for (auto &Address : Host.m_Addresses)
					{
						if (!Address.m_bConnected)
							DMibError("Found unconnected address");
					}			
				}
			}
		}
		
		CDistributedActorTestHelper::~CDistributedActorTestHelper()
		{
		}

		NStr::CStr CDistributedActorTestHelper::fp_Subscribe(NStr::CStr const &_Namespace, bool _bExpectFailure)
		{
			TCActor<CActorDistributionManager> &ClientManager = mp_Manager;
			
			auto &ConcurrentActor = fg_ConcurrentActor();
			
			NStr::CStr SubscriptionID = NCryptography::fg_RandomID();
			
			auto &Subscription = mp_Subscriptions[SubscriptionID];
			
			Subscription.m_Subscription = ClientManager
				(
					&CActorDistributionManager::f_SubscribeActors
					, NContainer::fg_CreateVector<NStr::CStr>(_Namespace)
					, ConcurrentActor 
					, [this, pSubscription = &Subscription](CAbstractDistributedActor &&_NewActor)
					{
						DMibLock(mp_RemoteLock);
						pSubscription->m_RemoteActor = _NewActor.f_GetActorUnsafe<CActor>();
						mp_RemoteEvent.f_Signal();
					}
					, [this, pSubscription = &Subscription](TCWeakDistributedActor<CActor> const &_RemovedActor)
					{
						DMibLock(mp_RemoteLock);
						if (_RemovedActor == pSubscription->m_RemoteActor)
							pSubscription->m_RemoteActor.f_Clear();
						mp_RemoteEvent.f_Signal();
					}
				).f_CallSync(60.0)
			;
			
			fp64 WaitTime = 60.0;
			if (_bExpectFailure)
				WaitTime = 2.0;
			
			bool bTimedOutWatingForActor = false;
			while (!bTimedOutWatingForActor)
			{
				{
					DMibLock(mp_RemoteLock);
					if (!Subscription.m_RemoteActor.f_IsEmpty())
						break;
				}
				bTimedOutWatingForActor = mp_RemoteEvent.f_WaitTimeout(WaitTime);
			}
			if (_bExpectFailure)
			{
				DMibTest(DMibExpr(bTimedOutWatingForActor) && DMibExpr(Subscription.m_RemoteActor.f_IsEmpty()));
				Subscription.m_Subscription.f_Clear();
				return NStr::CStr();
			}
			if (bTimedOutWatingForActor)
				DMibError("Timed out waiting for actor subscription");
			return SubscriptionID;
		}

		NStr::CStr CDistributedActorTestHelper::f_Subscribe(NStr::CStr const &_Namespace)
		{
			return fp_Subscribe(_Namespace, false);
		}
		
		void CDistributedActorTestHelper::f_SubscribeExpectFailure(NStr::CStr const &_Namespace)
		{
			fp_Subscribe(_Namespace, true);
		}
		
		void CDistributedActorTestHelper::f_Unsubscribe(NStr::CStr const &_Subscription)
		{
			auto *pSubscription = mp_Subscriptions.f_FindEqual(_Subscription);
			if (pSubscription)
				pSubscription->m_Subscription.f_Clear();
		}			
		
		void CDistributedActorTestHelper::f_Unpublish(NStr::CStr const &_Publication)
		{
			auto *pPublication = mp_Publications.f_FindEqual(_Publication);
			pPublication->m_Publication.f_Clear();
		}
		
		void CDistributedActorTestHelper::f_SetSecurity(CDistributedActorSecurity const &_Security)
		{
			mp_Manager(&CActorDistributionManager::f_SetSecurity, _Security).f_CallSync(60.0);
		}
		
		NStr::CStr const &CDistributedActorTestHelper::f_GetHostID() const
		{
			return mp_HostID; 
		}
		
		CDistributedActorTestHelper &CDistributedActorTestHelperCombined::f_GetServer()
		{
			return *mp_pServer;
		}
		
		CDistributedActorTestHelper &CDistributedActorTestHelperCombined::f_GetClient()
		{
			return *mp_pClient;
		}
	}
}
