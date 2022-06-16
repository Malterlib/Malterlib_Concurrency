// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Network/Sockets/SSL>

#include "Malterlib_Concurrency_DistributedActor_TestHelpers.h"
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Test/Test>

namespace NMib::NConcurrency
{
	CDistributedActorTestHelperCombined::CDistributedActorTestHelperCombined(NStr::CStr const &_ListenDirectory)
		: mp_ListenSettings({(NStr::CStr::CFormat("wss://[UNIX(666):{}]/") << NNetwork::fg_GetSafeUnixSocketPath(_ListenDirectory / "tests.sock")).f_GetStr()})
		, mp_ServerCryptography(NCryptography::fg_RandomID())
		, mp_ClientCryptography(NCryptography::fg_RandomID())
		, mp_ListenSocketPath(NStr::CStr::CFormat("UNIX(666):{}") << NNetwork::fg_GetSafeUnixSocketPath(_ListenDirectory / "tests.sock"))
	{
	}

	CDistributedActorTestHelperCombined::~CDistributedActorTestHelperCombined()
	{
	}

	NStr::CStr CDistributedActorTestHelperCombined::f_GetListenSocketPath() const
	{
		return mp_ListenSocketPath;
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

	void CDistributedActorTestHelperCombined::f_BreakClientConnection(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags)
	{
		mp_ClientConnectionReference.f_Debug_Break(_Timeout, _Flags).f_CallSync(60.0);
	}

	void CDistributedActorTestHelperCombined::f_BreakServerConnections(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags)
	{
		mp_ListenReference.f_Debug_BreakAllConnections(_Timeout, _Flags).f_CallSync(60.0);
	}

	void CDistributedActorTestHelperCombined::f_SeparateServerManager()
	{
		mp_pServer = fg_Construct
			(
				mp_ServerCryptography.m_HostID
				, fg_ConstructActor<CActorDistributionManager>(CActorDistributionManagerInitSettings{mp_ServerCryptography.m_HostID, {}})
			)
		;
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
			mp_ServerCryptography.m_HostID = fg_InitDistributionManager(CActorDistributionManagerInitSettings{mp_ServerCryptography.m_HostID, {}}).m_HostID;
			mp_pServer = fg_Construct(mp_ServerCryptography.m_HostID, fg_GetDistributionManager());
		}
		
		TCActor<CActorDistributionManager> const &ServerManager = mp_pServer->f_GetManager();
		
		mp_ServerCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), CDistributedActorTestKeySettings{});
		
		mp_ListenSettings.f_SetCryptography(mp_ServerCryptography);
		mp_ListenSettings.m_bRetryOnListenFailure = false;
		mp_ListenSettings.m_ListenFlags = NNetwork::ENetFlag_None;
		mp_ListenReference = ServerManager(&CActorDistributionManager::f_Listen, mp_ListenSettings).f_CallSync(60.0);
	}

	void CDistributedActorTestHelperCombined::f_DisconnectClient(bool _bNewExecutionID)
	{
		mp_ClientConnectionReference.f_Disconnect().f_CallSync(60.0);
		if (_bNewExecutionID)
		{
			if (mp_pClient)
			{
				mp_pClient->f_Destroy();
				mp_pClient.f_Clear();
			}
		}
	}

	void CDistributedActorTestHelperCombined::f_InitClient(CDistributedActorTestHelperCombined &_Server)
	{
		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = mp_ListenSettings.m_ListenAddresses[0];
		ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
		if (!mp_pClient)
		{
			mp_pClient = fg_Construct
				(
					mp_ClientCryptography.m_HostID
					, fg_ConstructActor<CActorDistributionManager>(CActorDistributionManagerInitSettings{mp_ClientCryptography.m_HostID, {}})
				)
			;
		}
		if (mp_ClientCryptography.m_RemoteClientCertificates.f_IsEmpty())
		{
			mp_ClientCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), CDistributedActorTestKeySettings{});
			auto CertificateRequest = mp_ClientCryptography.f_GenerateRequest();
			auto SignedRequest = _Server.mp_ServerCryptography.f_SignRequest(CertificateRequest);
			mp_ClientCryptography.f_AddRemoteServer(ConnectionSettings.m_ServerURL, _Server.mp_ServerCryptography.m_PublicCertificate, SignedRequest);
		}
		ConnectionSettings.f_SetCryptography(mp_ClientCryptography);
		ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		ConnectionSettings.m_bRetryConnectOnFailure = false;

		TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager(); 
		mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(60.0).m_ConnectionReference;
	}
	
	void CDistributedActorTestHelperCombined::f_InitAnonymousClient(CDistributedActorTestHelperCombined &_Server)
	{
		mp_pClient = fg_Construct
			(
				mp_ClientCryptography.m_HostID
				, fg_ConstructActor<CActorDistributionManager>(CActorDistributionManagerInitSettings{mp_ClientCryptography.m_HostID, {}})
			)
		;
		
		TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager(); 
		
		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = mp_ListenSettings.m_ListenAddresses[0];
		ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
		ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		ConnectionSettings.m_bRetryConnectOnFailure = false;

		mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(60.0).m_ConnectionReference;
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
		, mp_pRemoteLock(fg_Construct())
	{
	}
	
	CDistributedActorTestHelper::CDistributedActorTestHelper(TCActor<CDistributedActorTrustManager> const &_TrustManager, bool _bAllowUnconnected)
		: mp_HostID(_TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(60.0))
		, mp_Manager(_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(60.0))
		, mp_pRemoteLock(fg_Construct())
	{
		if (!_bAllowUnconnected)
		{
			auto State = _TrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(60.0);
			for (auto &Host : State.m_Hosts)
			{
				for (auto &ConnectionStates : Host.m_Addresses)
				{
					auto &Address = ConnectionStates.f_GetAddress();
					for (auto &State : ConnectionStates.m_States)
					{
						if (!State.m_bConnected)
							DMibError(fg_Format("Found unconnected address: {} - {}", Address, State.m_Error));
					}
				}			
			}
		}
	}
	
	CDistributedActorTestHelper::~CDistributedActorTestHelper()
	{
		DMibLock(*mp_pRemoteLock);
		mp_pDeleted->f_Exchange(true);
	}

	NStr::CStr CDistributedActorTestHelper::fp_Subscribe(NStr::CStr const &_Namespace, bool _bExpectFailure, mint _nExpected)
	{
		TCActor<CActorDistributionManager> &ClientManager = mp_Manager;
		
		NStr::CStr SubscriptionID = NCryptography::fg_RandomID(mp_Subscriptions);
		
		auto &Subscription = mp_Subscriptions[SubscriptionID];
		
		auto Cleanup = g_OnScopeExit / [&]
			{
				mp_Subscriptions.f_Remove(SubscriptionID);
			}
		;
		
		Subscription.m_Subscription = ClientManager
			(
				&CActorDistributionManager::f_SubscribeActors
				, _Namespace
				, mp_ProcessingActor 
				, 	
				[
					this
					, pRemoteLock = mp_pRemoteLock
					, pDeletedHelper = mp_pDeleted
					, pSubscription = &Subscription
					, pDeleted = Subscription.m_pDeleted
				]
				(CAbstractDistributedActor &&_NewActor)
				{
					DMibLock(*pRemoteLock);
					if (pDeletedHelper->f_Load())
						return;
					if (pDeleted->f_Load())
						return;
					pSubscription->m_RemoteActors.f_Insert(fg_Move(_NewActor));
					mp_RemoteEvent.f_Signal();
				}
				, 
				[
					this
					, pRemoteLock = mp_pRemoteLock
					, pDeletedHelper = mp_pDeleted
					, pSubscription = &Subscription
					, pDeleted = Subscription.m_pDeleted
				]
				(CDistributedActorIdentifier const &_RemovedActor)
				{
					DMibLock(*pRemoteLock);
					if (pDeletedHelper->f_Load())
						return;
					if (pDeleted->f_Load())
						return;
					mint nActors = pSubscription->m_RemoteActors.f_GetLen();
					for (mint iActor = 0; iActor < nActors; )
					{
						auto &RemoteActor = pSubscription->m_RemoteActors[iActor]; 
						if (RemoteActor.f_GetIdentifier() == _RemovedActor)
						{
							pSubscription->m_RemoteActors.f_Remove(iActor);
							--nActors;
							continue;
						}
						++iActor;
					}
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
				DMibLock(*mp_pRemoteLock);
				if (Subscription.m_RemoteActors.f_GetLen() == _nExpected)
					break;
			}
			bTimedOutWatingForActor = mp_RemoteEvent.f_WaitTimeout(WaitTime);
		}
		if (_bExpectFailure)
		{
#if DMibConfig_Tests_Enable

			DMibTest(DMibExpr(bTimedOutWatingForActor) && DMibExpr(Subscription.m_RemoteActors.f_IsEmpty()));
#endif
			Subscription.m_Subscription.f_Clear();
			return NStr::CStr();
		}
		if (bTimedOutWatingForActor)
			DMibError("Timed out waiting for actor subscription");
		Cleanup.f_Clear();
		return SubscriptionID;
	}

	NStr::CStr CDistributedActorTestHelper::f_Subscribe(NStr::CStr const &_Namespace, mint _nExpected)
	{
		return fp_Subscribe(_Namespace, false, _nExpected);
	}
	
	void CDistributedActorTestHelper::f_SubscribeExpectFailure(NStr::CStr const &_Namespace)
	{
		fp_Subscribe(_Namespace, true, 1);
	}
	
	void CDistributedActorTestHelper::f_Unsubscribe(NStr::CStr const &_Subscription)
	{
		auto *pSubscription = mp_Subscriptions.f_FindEqual(_Subscription);
		if (pSubscription)
		{
			if (pSubscription->m_Subscription)
				fg_Exchange(pSubscription->m_Subscription, nullptr)->f_Destroy().f_CallSync(60.0);
		}
	}

	void CDistributedActorTestHelper::f_Destroy()
	{
		TCActorResultVector<void> Results;
		for (auto &Subscription : mp_Subscriptions)
		{
			if (Subscription.m_Subscription)
				Subscription.m_Subscription->f_Destroy() > Results.f_AddResult();
		}

		for (auto &Publication : mp_Publications)
			Publication.m_Publication.f_Destroy() > Results.f_AddResult();

		Results.f_GetResults().f_CallSync(60.0);
	}

	void CDistributedActorTestHelper::f_Unpublish(NStr::CStr const &_Publication)
	{
		auto *pPublication = mp_Publications.f_FindEqual(_Publication);
		pPublication->m_Publication.f_Destroy().f_CallSync(60.0);
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
