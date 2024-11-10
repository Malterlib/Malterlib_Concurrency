// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Process/Platform>
#include <Mib/Cryptography/Certificate>
#include <Mib/Concurrency/LogError>

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
		TCFutureVector<void> Results;
		auto &Internal = *mp_pInternal;

		CLogError LogError("Mib/Concurrency/Actors");

		if (Internal.m_ResolveActor)
			fg_Move(Internal.m_ResolveActor).f_Destroy() > Results;

		if (Internal.m_WebsocketClientConnector)
			fg_Move(Internal.m_WebsocketClientConnector).f_Destroy() > Results;

		for (auto &Listen : Internal.m_Listens)
		{
			Listen.m_ListenCallbackSubscription.f_Clear();
			if (Listen.m_WebsocketServer)
				fg_Move(Listen.m_WebsocketServer).f_Destroy() > Results;
		}

		Internal.m_Listens.f_Clear();

		for (auto &pConnection : Internal.m_ClientConnections)
			pConnection->f_Disconnect() > Results;

		for (auto &pConnection : Internal.m_ServerConnections)
			pConnection->f_Disconnect() > Results;

		if (Internal.m_CleanupTimerSubscription)
			Internal.m_CleanupTimerSubscription->f_Destroy() > Results;

		co_await fg_AllDone(Results).f_Wrap() > LogError.f_Warning("Failed to destroy actor distribution manager");

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
			bool bHandled = NException::fg_VisitException<CExceptionActorDeleted, NException::CException>
				(
					_Result.f_GetException()
					, [&]<typename tf_CException>(tf_CException const &_Exception)
					{
						if constexpr (NTraits::TCIsSame<tf_CException, CExceptionActorDeleted>::mc_Value)
						{
							// Ignore already deleted close
						}
						else
							DMibLogWithCategory(Mib/Concurrency/Actors, Info, "<{}> Closed with error '{}' {{{}}: {}", _Desc, _ServerURL, _ConnectionID, _Exception.f_GetErrorStr());
					}
				)
			;

			if (!bHandled)
				_Result.f_Access();
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
		NException::CExceptionPointer pException;
		auto fGetExceptionPointer = [&]
			{
				if (pException)
					return pException;

				pException = DMibErrorInstance(_Error).f_ExceptionPointer();
				return pException;
			}
		;
		m_IdentifyPromise.f_Abandon(fGetExceptionPointer);

		for (auto &Finished : m_PublishFinished)
			Finished.f_Abandon(fGetExceptionPointer);

		m_PublishFinished.f_Clear();
	}

	TCFuture<void> CActorDistributionManagerInternal::CConnection::f_Disconnect()
	{
		return g_ConcurrentDispatch /
			[
				bIsLastConnection = m_HostLink.f_IsAloneInList()
				, ConnectionID = f_GetConnectionID()
				, ServerURL = f_GetServerURL()
				, Desc = f_GetHostInfo().f_GetDesc()
				, Connection = fg_Move(m_Connection)
			]
			() mutable -> TCFuture<void>
			{
				if (!Connection)
					co_return {};

				auto Result = co_await Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, "Normal disconnect", 5.0).f_Wrap();
				fs_LogClose(Result, bIsLastConnection, ConnectionID, ServerURL, Desc);

				if (!Result)
					co_return Result.f_GetException();
				
				co_return {};
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

			TCPromise<void> Promise{CPromiseConstructEmpty()};
			if (_pPromise)
				Promise = fg_Move(*_pPromise);

			m_Connection(&NWeb::CWebSocketActor::f_CloseWithLinger, NWeb::EWebSocketStatus_NormalClosure, _Message.f_Left(NWeb::CWebSocketActor::mc_MaxCloseMessageLength), 5.0)
				> fg_ConcurrentActor() / [=, Promise = fg_Move(Promise)](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result) mutable
				{
					fs_LogClose(_Result, bIsLastConnection, ConnectionID, ServerURL, Desc);
					if (Promise.f_IsValid())
						Promise.f_SetResult();
				}
			;
			m_Connection.f_Clear();
		}
		else if (_pPromise)
			_pPromise->f_SetResult();

		if (m_pHost)
		{
			auto &Host = *m_pHost;
			if (Host.m_pLastSendConnection == this)
				Host.m_pLastSendConnection = nullptr;

			_This.fp_NotifyDisconnect(Host);
		}

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
		, m_ReconnectDelay(_InitSettings.m_ReconnectDelay)
		, m_HostDaemonTimeout(_InitSettings.m_HostDaemonTimeout)
	{
		m_WebsocketSettings.m_bTimeoutForUnixSockets = _InitSettings.m_bTimeoutForUnixSockets;

		if (m_FriendlyName.f_IsEmpty())
			m_FriendlyName = fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName());
	}

	CActorDistributionManagerInternal::~CActorDistributionManagerInternal()
	{
		while (auto *pHost = m_Hosts.f_FindAny())
			fp_DestroyHost(**pHost, nullptr, "Distribution manager was destroyed");
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
