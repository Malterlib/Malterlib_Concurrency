// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Network/Sockets/SSL>

#include "Malterlib_Concurrency_DistributedActor_TestHelpers.h"
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Test/Test>

namespace NMib::NConcurrency
{
	CActorRunLoopTestHelper::~CActorRunLoopTestHelper()
	{
		m_CurrentActor.f_Clear();

		m_HelperActor->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
		m_HelperActor.f_Clear();

		while (m_pRunLoop->m_RefCount.f_Get() > 0)
			m_pRunLoop->f_WaitOnceTimeout(0.1);

		m_pRunLoop.f_Clear();
	}

	CDistributedActorTestHelperCombined::CDistributedActorTestHelperCombined(NStr::CStr const &_ListenDirectory, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop)
		: mp_pRunLoop(_pRunLoop)
		, mp_ListenSettings({(NStr::CStr::CFormat("wss://[UNIX(666):{}]/") << NNetwork::fg_GetSafeUnixSocketPath(_ListenDirectory / "tests.sock")).f_GetStr()})
		, mp_ServerCryptography(NCryptography::fg_RandomID())
		, mp_ClientCryptography(NCryptography::fg_RandomID())
		, mp_ListenSocketPath(NStr::CStr::CFormat("UNIX(666):{}") << NNetwork::fg_GetSafeUnixSocketPath(_ListenDirectory / "tests.sock"))
	{
		NTest::fg_TestAddCleanupPath(_ListenDirectory);
	}

	CDistributedActorTestHelperCombined::~CDistributedActorTestHelperCombined()
	{
		// Stop client connection and listen socket, waiting for completion
		// This prevents race conditions with subsequent tests using the same socket path
		TCFutureVector<void> Futures;

		if (mp_ClientConnectionReference.f_IsValid())
			mp_ClientConnectionReference.f_Disconnect() > Futures;

		// f_Stop() returns an error if already stopped, so wrap to ignore errors
		mp_ListenReference.f_Stop() > Futures;

		if (mp_pClient)
			mp_pClient->f_Destroy();

		if (mp_pServer)
			mp_pServer->f_Destroy();

		try
		{
			fg_AllDoneWrapped(Futures).f_CallSync(mp_pRunLoop, 60.0);
		}
		catch (...)
		{
		}
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

#if DMibConfig_Tests_Enable
	void CDistributedActorTestHelperCombined::f_BreakClientConnection(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags)
	{
		mp_ClientConnectionReference.f_Debug_Break(_Timeout, _Flags).f_CallSync(mp_pRunLoop, 60.0);
	}

	void CDistributedActorTestHelperCombined::f_BreakServerConnections(fp64 _Timeout, NNetwork::ESocketDebugFlag _Flags)
	{
		mp_ListenReference.f_Debug_BreakAllConnections(_Timeout, _Flags).f_CallSync(mp_pRunLoop, 60.0);
	}
#endif

	void CDistributedActorTestHelperCombined::f_SeparateServerManager()
	{
		CActorDistributionManagerInitSettings Settings{mp_ServerCryptography.m_HostID, {}};
		Settings.m_ReconnectDelay = 1_ms;
		Settings.m_FriendlyName = "TestHelperServer";

		mp_pServer = fg_Construct
			(
				mp_ServerCryptography.m_HostID
				, fg_ConstructActor<CActorDistributionManager>(Settings)
				, mp_pRunLoop
			)
		;
	}

	void CDistributedActorTestHelperCombined::f_Init(bool _bReconnect)
	{
		f_InitServer();
		f_InitClient(*this, _bReconnect);
	}

	void CDistributedActorTestHelperCombined::f_InitServer()
	{
		if (!mp_pServer)
		{
			CActorDistributionManagerInitSettings Settings{mp_ServerCryptography.m_HostID, {}};
			Settings.m_ReconnectDelay = 1_ms;
			Settings.m_FriendlyName = "TestHelperServer";

			mp_ServerCryptography.m_HostID = fg_InitDistributionManager(Settings).m_HostID;
			mp_pServer = fg_Construct(mp_ServerCryptography.m_HostID, fg_GetDistributionManager(), mp_pRunLoop);
		}

		TCActor<CActorDistributionManager> const &ServerManager = mp_pServer->f_GetManager();

		mp_ServerCryptography.f_GenerateNewCert(NContainer::fg_CreateVector<NStr::CStr>("localhost"), CDistributedActorTestKeySettings{});

		mp_ListenSettings.f_SetCryptography(mp_ServerCryptography);
		mp_ListenSettings.m_bRetryOnListenFailure = false;
		mp_ListenSettings.m_ListenFlags = NNetwork::ENetFlag_None;
		mp_ListenReference = ServerManager(&CActorDistributionManager::f_Listen, mp_ListenSettings).f_CallSync(mp_pRunLoop, 60.0);
	}

	void CDistributedActorTestHelperCombined::f_DisconnectClient(bool _bNewExecutionID)
	{
		mp_ClientConnectionReference.f_Disconnect().f_CallSync(mp_pRunLoop, 60.0);
		if (_bNewExecutionID)
		{
			if (mp_pClient)
			{
				mp_pClient->f_Destroy();
				mp_pClient.f_Clear();
			}
		}
	}

	void CDistributedActorTestHelperCombined::f_InitClient(CDistributedActorTestHelperCombined &_Server, bool _bReconnect)
	{
		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = _Server.mp_ListenSettings.m_ListenAddresses[0];
		ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
		if (!mp_pClient)
		{
			CActorDistributionManagerInitSettings Settings{mp_ClientCryptography.m_HostID, {}};
			Settings.m_ReconnectDelay = 1_ms;
			Settings.m_FriendlyName = "TestHelperClient";

			mp_pClient = fg_Construct
				(
					mp_ClientCryptography.m_HostID
					, fg_ConstructActor<CActorDistributionManager>(Settings)
					, mp_pRunLoop
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
		ConnectionSettings.m_bRetryConnectOnFailure = _bReconnect;

		TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager();
		mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(mp_pRunLoop, 60.0).m_ConnectionReference;
	}

	void CDistributedActorTestHelperCombined::f_InitAnonymousClient(CDistributedActorTestHelperCombined &_Server)
	{
		{
			CActorDistributionManagerInitSettings Settings{mp_ClientCryptography.m_HostID, {}};
			Settings.m_ReconnectDelay = 1_ms;
			Settings.m_FriendlyName = "TestHelperAnonClient";

			mp_pClient = fg_Construct
				(
					mp_ClientCryptography.m_HostID
					, fg_ConstructActor<CActorDistributionManager>(Settings)
					, mp_pRunLoop
				)
			;
		}

		TCActor<CActorDistributionManager> const &ClientManager = mp_pClient->f_GetManager();

		CActorDistributionConnectionSettings ConnectionSettings;
		ConnectionSettings.m_ServerURL = _Server.mp_ListenSettings.m_ListenAddresses[0];
		ConnectionSettings.m_PublicServerCertificate = _Server.mp_ListenSettings.m_CACertificate;
		ConnectionSettings.m_bRetryConnectOnFirstFailure = false;
		ConnectionSettings.m_bRetryConnectOnFailure = false;

		mp_ClientConnectionReference = ClientManager(&CActorDistributionManager::f_Connect, ConnectionSettings, 60.0).f_CallSync(mp_pRunLoop, 60.0).m_ConnectionReference;
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

	void CDistributedActorTestHelperCombined::f_Republish(NStr::CStr const &_Publication, NStr::CStr const &_HostID, fp32 _Timeout)
	{
		mp_pServer->f_Republish(_Publication, _HostID, _Timeout);
	}

	TCActor<CActorDistributionManager> const &CDistributedActorTestHelper::f_GetManager() const
	{
		return mp_Manager;
	}

	CDistributedActorTestHelper::CDistributedActorTestHelper
		(
			NStr::CStr const &_HostID
			, TCActor<CActorDistributionManager> const &_Manager
			, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
		)
		: mp_pRunLoop(_pRunLoop)
		, mp_HostID(_HostID)
		, mp_Manager(_Manager)
		, mp_pRemoteLock(fg_Construct())
	{
	}

	CDistributedActorTestHelper::CDistributedActorTestHelper
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
			, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
			, bool _bAllowUnconnected
		)
		: mp_HostID(_TrustManager(&CDistributedActorTrustManager::f_GetHostID).f_CallSync(mp_pRunLoop, 60.0))
		, mp_Manager(_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(mp_pRunLoop, 60.0))
		, mp_pRemoteLock(fg_Construct())
		, mp_pRunLoop(_pRunLoop)
	{
		if (!_bAllowUnconnected)
		{
			auto State = _TrustManager(&CDistributedActorTrustManager::f_GetConnectionState).f_CallSync(mp_pRunLoop, 60.0);
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

	NStr::CStr CDistributedActorTestHelper::fp_Subscribe(NStr::CStr const &_Namespace, bool _bExpectFailure, umint _nExpected)
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
				(CAbstractDistributedActor _NewActor) -> TCFuture<void>
				{
					DMibLock(*pRemoteLock);
					if (pDeletedHelper->f_Load())
						co_return {};
					if (pDeleted->f_Load())
						co_return {};

					pSubscription->m_RemoteActors.f_Insert(fg_Move(_NewActor));
					mp_RemoteEvent.f_Signal();

					co_return {};
				}
				,
				[
					this
					, pRemoteLock = mp_pRemoteLock
					, pDeletedHelper = mp_pDeleted
					, pSubscription = &Subscription
					, pDeleted = Subscription.m_pDeleted
				]
				(CDistributedActorIdentifier _RemovedActor) -> TCFuture<void>
				{
					DMibLock(*pRemoteLock);
					if (pDeletedHelper->f_Load())
						co_return {};
					if (pDeleted->f_Load())
						co_return {};
					umint nActors = pSubscription->m_RemoteActors.f_GetLen();
					for (umint iActor = 0; iActor < nActors; )
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

					co_return {};
				}
			).f_CallSync(mp_pRunLoop, 60.0)
		;

		fp64 WaitTime = 60.0;
		if (_bExpectFailure)
			WaitTime = 0.5;

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

	NStr::CStr CDistributedActorTestHelper::f_Subscribe(NStr::CStr const &_Namespace, umint _nExpected)
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
				fg_Exchange(pSubscription->m_Subscription, nullptr)->f_Destroy().f_CallSync(mp_pRunLoop, 60.0);
		}
	}

	void CDistributedActorTestHelper::f_Destroy()
	{
		TCFutureVector<void> Results;
		for (auto &Subscription : mp_Subscriptions)
		{
			if (Subscription.m_Subscription)
				Subscription.m_Subscription->f_Destroy() > Results;
		}

		for (auto &Publication : mp_Publications)
			Publication.m_Publication.f_Destroy() > Results;

		fg_AllDone(Results).f_CallSync(mp_pRunLoop, 60.0);
	}

	void CDistributedActorTestHelper::f_Unpublish(NStr::CStr const &_Publication)
	{
		auto *pPublication = mp_Publications.f_FindEqual(_Publication);
		pPublication->m_Publication.f_Destroy().f_CallSync(mp_pRunLoop, 60.0);
	}

	void CDistributedActorTestHelper::f_Republish(NStr::CStr const &_Publication, NStr::CStr const &_HostID, fp32 _Timeout)
	{
		auto *pPublication = mp_Publications.f_FindEqual(_Publication);
		pPublication->m_Publication.f_Republish(_HostID, _Timeout).f_CallSync(mp_pRunLoop, 60.0);
	}

	void CDistributedActorTestHelper::f_SetSecurity(CDistributedActorSecurity const &_Security)
	{
		mp_Manager(&CActorDistributionManager::f_SetSecurity, _Security).f_CallSync(mp_pRunLoop, 60.0);
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
