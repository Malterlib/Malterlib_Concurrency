// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Process/Platform>
#include <Mib/Cryptography/Certificate>

namespace NMib::NConcurrency
{
	CActorDistributionManagerInitSettings::CActorDistributionManagerInitSettings(NStr::CStr const &_HostID, NStr::CStr const &_Enclave, NStr::CStr const &_FriendlyName)
		: m_HostID(_HostID)
		, m_Enclave(_Enclave)
		, m_FriendlyName(_FriendlyName)
	{
	}

	CActorDistributionManagerInitSettings::CActorDistributionManagerInitSettings(NStr::CStr const &_HostID, NStr::CStr const &_Enclave)
		: CActorDistributionManagerInitSettings{_HostID, _Enclave, fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName())}
	{
	}

	CActorDistributionManager::CActorDistributionManager(CActorDistributionManagerInitSettings const &_InitSettings)
		: mp_pInternal(fg_Construct(this, _InitSettings))
	{
		auto &Internal = *mp_pInternal;
		Internal.m_ResolveActor = fg_ConstructActor<NNetwork::CResolveActor>();
	}

	TCFuture<void> CActorDistributionManager::fp_Destroy()
	{
		TCActorResultVector<void> Results;
		auto &Internal = *mp_pInternal;

		if (Internal.m_ResolveActor)
			fg_Move(Internal.m_ResolveActor).f_Destroy() > Results.f_AddResult();

		if (Internal.m_WebsocketClientConnector)
			fg_Move(Internal.m_WebsocketClientConnector).f_Destroy() > Results.f_AddResult();

		for (auto &Listen : Internal.m_Listens)
		{
			Listen.m_ListenCallbackSubscription.f_Clear();
			if (Listen.m_WebsocketServer)
				fg_Move(Listen.m_WebsocketServer).f_Destroy() > Results.f_AddResult();
		}

		for (auto &pConnection : Internal.m_ClientConnections)
			pConnection->f_Disconnect() > Results.f_AddResult();

		for (auto &pConnection : Internal.m_ServerConnections)
			pConnection->f_Disconnect() > Results.f_AddResult();

		if (Internal.m_CleanupTimerSubscription)
			Internal.m_CleanupTimerSubscription->f_Destroy() > Results.f_AddResult();

		co_await fg_ExchangeMove(Results).f_GetResults();

		co_return {};
	}

	void CActorDistributionManagerInternal::CConnection::fs_LogClose
		(
			TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> const &_Result
			, bool _bIsLastConnection
			, NStr::CStr const &_ConnectionID
			, NStr::CStr const &_ServerURL
			, NStr::CStr const &_Desc
		)
	{
		if (_Result)
		{
			if (_Result->m_Status != NWeb::EWebSocketStatus_AlreadyClosed)
			{
				if (_Result->m_Status != NWeb::EWebSocketStatus_NormalClosure || _bIsLastConnection)
					DMibLogWithCategory(Mib/Concurrency/Actors, Info, "<{}> Closed '{}' {{{}}: {} - {}", _Desc, _ServerURL, _ConnectionID, _Result->m_Status, _Result->m_Reason);
				else
					DMibLogWithCategory(Mib/Concurrency/Actors, DebugVerbose1, "<{}> Closed '{}' {{{}}: {} - {}", _Desc, _ServerURL, _ConnectionID, _Result->m_Status, _Result->m_Reason);
			}
		}
		else
		{
			try
			{
				_Result.f_Access();
			}
			catch (CExceptionActorDeleted const &)
			{
				// Ignore already deleted close
			}
			catch (NException::CException const &)
			{
				DMibLogWithCategory(Mib/Concurrency/Actors, Info, "<{}> Closed with error '{}' {{{}}: {}", _Desc, _ServerURL, _ConnectionID, _Result.f_GetExceptionStr());
			}
		}
	}

	CHostInfo CActorDistributionManagerInternal::CConnection::f_GetHostInfo() const
	{
		CHostInfo Info;

		if (m_pHost)
		{
			Info.m_HostID = m_pHost->m_HostInfo.m_RealHostID;
			Info.m_FriendlyName = m_pHost->m_FriendlyName;
		}
		else
			Info.m_HostID = "Unknown";

		return Info;
	}

	void CActorDistributionManagerInternal::CConnection::f_DiscardIdentifyPromise(NStr::CStr const &_Error)
	{
		m_IdentifyPromise.f_Abandon
			(
			 	[&]()
			 	{
					return DMibErrorInstance(_Error).f_ExceptionPointer();
				}
			)
		;
		for (auto &Finished : m_PublishFinished)
		{
			Finished.f_Abandon
				(
			 		[&]()
			 		{
						return DMibErrorInstance(_Error).f_ExceptionPointer();
					}
				)
			;
		}
		m_PublishFinished.f_Clear();
	}

	TCFuture<void> CActorDistributionManagerInternal::CConnection::f_Disconnect()
	{
		return g_Future <<= g_ConcurrentDispatch /
			[
				bIsLastConnection = m_HostLink.f_IsAloneInList()
				, ConnectionID = f_GetConnectionID()
				, ServerURL = f_GetServerURL()
				, Desc = f_GetHostInfo().f_GetDesc()
				, Connection = fg_Move(m_Connection)
			]
			() mutable
			{
				TCPromise<void> Promise;
				if (!Connection)
					return Promise <<= g_Void;

				Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, "Normal disconnect", 5.0)
					> fg_ConcurrentActor() / [=, Connection = Connection](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result)
					{
						fs_LogClose(_Result, bIsLastConnection, ConnectionID, ServerURL, Desc);
						if (_Result)
							Promise.f_SetResult();
						else
							Promise.f_SetException(fg_Move(_Result));
					}
				;
				return Promise.f_MoveFuture();
			}
		;
	}

	void CActorDistributionManagerInternal::CConnection::f_Reset(bool _bResetHost, CActorDistributionManagerInternal &_This, NStr::CStr const &_Message, TCPromise<void> *_pPromise)
	{
		using namespace NStr;
		f_DiscardIdentifyPromise("Connection reset: {}"_f << _Message);
		m_Link.f_Unlink();
		bool bIsLastConnection = m_HostLink.f_IsAloneInList();

		if (m_pHost && m_pHost->m_ActiveConnections.f_IsEmpty())
			_This.fp_CleanupMarkInactive(*m_pHost);

		if (_bResetHost)
			m_HostLink.f_Unlink();
		m_ConnectionSubscription.f_Clear();
		if (m_Connection)
		{
			auto ConnectionID = f_GetConnectionID();
			auto ServerURL = f_GetServerURL();
			auto Desc = f_GetHostInfo().f_GetDesc();

			NStorage::TCOptional<TCPromise<void>> Promise;
			if (_pPromise)
				Promise = fg_Move(*_pPromise);

			m_Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, _Message, 5.0)
				> fg_ConcurrentActor() / [=, Promise = fg_Move(Promise)](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result) mutable
				{
					fs_LogClose(_Result, bIsLastConnection, ConnectionID, ServerURL, Desc);
					if (Promise)
						Promise->f_SetResult();
				}
			;
			m_Connection.f_Clear();
		}
		else if (_pPromise)
			_pPromise->f_SetResult();

		if (m_pHost && m_pHost->m_pLastSendConnection == this)
			m_pHost->m_pLastSendConnection = nullptr;
		if (_bResetHost)
			m_pHost.f_Clear();
	}

	void CActorDistributionManagerInternal::CConnection::f_Destroy(NStr::CStr const &_Message, CActorDistributionManagerInternal &_This, TCPromise<void> *_pPromise)
	{
		f_DiscardIdentifyPromise(fg_Format("Connection destroyed: {}", _Message));
		f_Reset(true, _This, _Message, _pPromise);
		m_pSSLContext.f_Clear();
	}

	NStr::CStr CActorDistributionManagerInternal::CConnection::f_GetServerURL() const
	{
		return "Unknown";
	}

	void CActorDistributionManagerInternal::CClientConnection::f_Reset(bool _bResetHost, CActorDistributionManagerInternal &_This, NStr::CStr const &_Message)
	{
		CConnection::f_Reset(_bResetHost, _This, _Message, nullptr);
		m_bConnected = false;
	}

	void CActorDistributionManagerInternal::CClientConnection::f_Destroy(NStr::CStr const &_Message, CActorDistributionManagerInternal &_This, TCPromise<void> *_pPromise)
	{
		CConnection::f_Destroy(_Message, _This, _pPromise);
		m_bConnected = false;
	}

	NStr::CStr CActorDistributionManagerInternal::CClientConnection::f_GetConnectionID() const
	{
		return m_ConnectionID;
	}

	NStr::CStr CActorDistributionManagerInternal::CClientConnection::f_GetServerURL() const
	{
		return m_ServerURL.f_Encode();
	}

	void CActorDistributionManagerInternal::CClientConnection::f_SetLastError(NStr::CStr const &_Error)
	{
		m_LastConnectionError = _Error;
		m_LastConnectionErrorTime = NTime::CTime::fs_NowUTC();

		if (m_pHost)
		{
			m_pHost->m_LastError = _Error;
			m_pHost->m_LastErrorTime = m_LastConnectionErrorTime;
		}
	}

	NActorDistributionManagerInternal::CServerConnection::CServerConnection(mint _ConnectionID)
		: m_ConnectionID(_ConnectionID)
	{
	}

	NStr::CStr NActorDistributionManagerInternal::CServerConnection::f_GetConnectionID() const
	{
		return NStr::fg_Format("{}", m_ConnectionID);
	}

	CActorDistributionManager::~CActorDistributionManager()
	{
		auto &Internal = *mp_pInternal;

		for (auto &pConnection : Internal.m_ClientConnections)
		{
			++pConnection->m_ConnectionSequence;
			pConnection->f_Destroy("", Internal, nullptr);
		}
		for (auto &pConnection : Internal.m_ServerConnections)
			pConnection->f_Destroy("", Internal, nullptr);
	}

	CActorDistributionManagerInternal::CActorDistributionManagerInternal(CActorDistributionManager *_pThis, CActorDistributionManagerInitSettings const &_InitSettings)
		: m_pThis(_pThis)
		, m_FriendlyName(_InitSettings.m_FriendlyName)
		, m_HostID(_InitSettings.m_HostID)
		, m_Enclave(_InitSettings.m_Enclave)
		, m_HostTimeout(_InitSettings.m_HostTimeout)
		, m_HostDaemonTimeout(_InitSettings.m_HostDaemonTimeout)
	{
		if (m_FriendlyName.f_IsEmpty())
			m_FriendlyName = fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName());
	}

	CActorDistributionManagerInternal::~CActorDistributionManagerInternal()
	{
		while (auto *pHost = m_Hosts.f_FindAny())
			fp_DestroyHost(**pHost, nullptr, "distribution manager was destroyed");
	}

	CActorDistributionManagerInternal::COnHostInfoChanged::~COnHostInfoChanged()
	{
		*m_pDestroyed = true;
	}

	namespace
	{
		bool fg_IsHostIDValid(NStr::CStr const &_HostID)
		{
			if (_HostID.f_GetLen() < 17 || _HostID.f_GetLen() > 128)
				return false;
			if (!_HostID.f_IsAnsiAlphaNumeric())
				return false;
			return true;
		}
	}

	bool CActorDistributionManager::fs_IsValidEnclave(NStr::CStr const &_String)
	{
		return fg_IsHostIDValid(_String);
	}

	NStr::CStr CActorDistributionManager::fs_GetCertificateHostID(NContainer::CByteVector const &_Certificate)
	{
		auto Extensions = NCryptography::CCertificate::fs_GetCertificateExtensions(_Certificate);

		auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
		if (!pHostIDExtension || pHostIDExtension->f_GetLen() != 1)
			return {};
		NStr::CStr HostID = (*pHostIDExtension)[0].m_Value;
		if (!fg_IsHostIDValid(HostID))
			return {};
		return HostID;
	}

	NStr::CStr CActorDistributionManager::fs_GetCertificateRequestHostID(NContainer::CByteVector const &_Certificate)
	{
		auto Extensions = NCryptography::CCertificate::fs_GetCertificateRequestExtensions(_Certificate);

		auto *pHostIDExtension = Extensions.f_FindEqual("MalterlibHostID");
		if (!pHostIDExtension || pHostIDExtension->f_GetLen() != 1)
			return {};
		NStr::CStr HostID = (*pHostIDExtension)[0].m_Value;
		if (!fg_IsHostIDValid(HostID))
			return {};
		return HostID;
	}
}
