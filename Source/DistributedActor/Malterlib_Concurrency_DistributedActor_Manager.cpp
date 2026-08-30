// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Process/Platform>
#include <Mib/Cryptography/Certificate>
#include <Mib/Concurrency/LogError>

namespace NMib::NConcurrency
{
	namespace NActorDistributionManagerInternal
	{
		// The wsa scheme selects the authenticated unix handshake instead of TLS; schemes preserve case so
		// the comparison must not, or a mixed case scheme would silently fall back to raw TCP
		bool fg_IsAuthenticatedUnixScheme(NStr::CStr const &_Scheme)
		{
			return NStr::fg_StrCmpNoCase(_Scheme, "wsa") == 0;
		}

		// Whitelists come from the domain's one configured key setting, not the local leaf, which
		// can lag it after a reconfiguration. A leaf's signature digest follows the issuer's key
		// rather than the leaf's, and an authority persisted under an earlier key setting keeps
		// signing with its own, so the authority's digest — the one on its self-signature — is
		// allowed alongside the setting's. An anonymous endpoint (no local certificate) leaves
		// the whitelists empty and relies on the pinned CA
		NCryptography::CCertificateVerifyOptions fg_VerifyOptionsFromKeySetting(NCryptography::CPublicKeySetting const &_KeySetting, bool _bHasLocalCertificate, NContainer::CByteVector const &_AuthorityCertificate)
		{
			NCryptography::CCertificateVerifyOptions VerifyOptions;
			if (!_bHasLocalCertificate)
				return VerifyOptions;

			VerifyOptions.m_AllowedLeafKeyTypes.f_Insert(_KeySetting);

			auto SettingDigest = NCryptography::fg_GetAutomaticDigestType(_KeySetting);
			VerifyOptions.m_AllowedSignatureDigests.f_Insert(SettingDigest);

			if (!_AuthorityCertificate.f_IsEmpty())
			{
				auto AuthorityDigest = NCryptography::CCertificate::fs_GetSignatureDigestType(_AuthorityCertificate);
				if (AuthorityDigest != NCryptography::EDigestType_None && AuthorityDigest != SettingDigest)
					VerifyOptions.m_AllowedSignatureDigests.f_Insert(AuthorityDigest);
			}

			return VerifyOptions;
		}

#if DMibConfig_IoDebug_Enable
		umint fg_TransportFragmentationOverride()
		{
			static umint s_nBytes =
				(
					[]() -> umint
					{
						auto Setting = NSys::fg_Process_GetEnvironmentVariable_NonProtected(NStr::gc_Str<"MalterlibTransportFragmentation">.m_Str);

						return Setting.f_ToIntExact(umint(0));
					}
					()
				)
			;

			return s_nBytes;
		}
#endif
	}

	using namespace NActorDistributionManagerInternal;

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
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/Actors
							, SubscriptionLogVerbosity
							, "<{}> Closed '{}' {{{}}: {} - {}"
							, _Desc
							, _ServerURL
							, _ConnectionID
							, _Result->m_Status
							, _Result->m_Reason
						)
					;
				}
			}
		}
		else
		{
			bool bHandled = NException::fg_VisitException<CExceptionActorDeleted, CExceptionActorAlreadyDestroyed, NException::CException>
				(
					_Result.f_GetException()
					, [&]<typename tf_CException>(tf_CException const &_Exception)
					{
						if constexpr (NTraits::cIsSame<tf_CException, CExceptionActorDeleted> || NTraits::cIsSame<tf_CException, CExceptionActorAlreadyDestroyed>)
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
				.f_OnResultSet
				(
					[=, Promise = fg_Move(Promise)](TCAsyncResult<NWeb::CWebSocketActor::CCloseInfo> &&_Result) mutable
					{
						fs_LogClose(_Result, bIsLastConnection, ConnectionID, ServerURL, Desc);
						if (Promise.f_IsValid())
							Promise.f_SetResult();
					}
				)
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
		// The base clears m_pSSLContext; release the authenticated unix context here too so its copied
		// private-key material is not retained past teardown (for example while a reconnect timer holds
		// this connection alive)
		m_pAuthenticatedUnixContext.f_Clear();
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

	CServerConnection::CServerConnection(umint _ConnectionID)
		: m_ConnectionID(_ConnectionID)
	{
	}

	NStr::CStr CServerConnection::f_GetConnectionID() const
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

	namespace
	{
		struct CSendWindowUnit
		{
			ch8 const *m_pSuffix;
			fp64 m_Scale;
		};

		// Longer suffixes first, so "kib" is not taken for "b" and "kbit" not for "bit"
		constexpr CSendWindowUnit gc_SendWindowByteUnits[] =
			{
				{"gib", 1024.0 * 1024.0 * 1024.0}, {"mib", 1024.0 * 1024.0}, {"kib", 1024.0}
				, {"gb", 1000.0 * 1000.0 * 1000.0}, {"mb", 1000.0 * 1000.0}, {"kb", 1000.0}
				, {"g", 1024.0 * 1024.0 * 1024.0}, {"m", 1024.0 * 1024.0}, {"k", 1024.0}, {"b", 1.0}
			}
		;

		constexpr CSendWindowUnit gc_SendWindowRateUnits[] =
			{
				{"gbps", 1000.0 * 1000.0 * 1000.0}, {"mbps", 1000.0 * 1000.0}, {"kbps", 1000.0}, {"bps", 1.0}
				, {"gbit", 1000.0 * 1000.0 * 1000.0}, {"mbit", 1000.0 * 1000.0}, {"kbit", 1000.0}, {"bit", 1.0}
			}
		;

		constexpr CSendWindowUnit gc_SendWindowLatencyUnits[] =
			{
				{"us", 0.000001}, {"ms", 0.001}, {"s", 1.0}
			}
		;

		// A number with one of the units as suffix, or without a unit at the scale given for that
		template <umint t_nUnits>
		bool fg_ParseScaled(NStr::CStr const &_Text, CSendWindowUnit const (&_Units)[t_nUnits], fp64 _UnitlessScale, fp64 &o_Value)
		{
			NStr::CStr Number = _Text.f_Trim();
			fp64 Scale = _UnitlessScale;
			for (auto const &Unit : _Units)
			{
				if (Number.f_EndsWithNoCase(Unit.m_pSuffix))
				{
					Number = Number.f_Left(Number.f_GetLen() - NStr::fg_StrLen(Unit.m_pSuffix)).f_Trim();
					Scale = Unit.m_Scale;
					break;
				}
			}

			if (Number.f_IsEmpty())
				return false;

			// The whole remainder must be the number: a stray word is a mistake, not a count, and
			// so is a second point, which the float parser would stop at rather than refuse
			ch8 const *pParse = Number.f_GetStr();
			bool bPoint = false;
			while (*pParse)
			{
				if (*pParse == '.')
				{
					if (bPoint)
						return false;

					bPoint = true;
				}
				else if (!NStr::fg_CharIsNumber(*pParse))
					return false;
				++pParse;
			}

			fp64 Value = Number.f_ToFloat(fp64(-1.0));
			if (Value < 0.0)
				return false;

			o_Value = Value * Scale;

			return true;
		}
	}

	bool fg_ParseSendWindow(NStr::CStr const &_Text, uint64 &o_Bytes, NStr::CStr &o_Error)
	{
		NStr::CStr Text = _Text.f_Trim();
		if (Text.f_IsEmpty() || (Text.f_GetLen() == 7 && Text.f_StartsWithNoCase("default")))
		{
			o_Bytes = 0;

			return true;
		}

		fp64 Bytes = 0.0;
		aint iAt = Text.f_Find("@");
		if (iAt >= 0)
		{
			fp64 BitsPerSecond = 0.0;
			if (!fg_ParseScaled(Text.f_Left(umint(iAt)), gc_SendWindowRateUnits, 1.0, BitsPerSecond))
			{
				o_Error = "Send window rate must be a number in bit, kbit, mbit or gbit per second, like 10gbit@10ms";

				return false;
			}

			fp64 LatencySeconds = 0.0;
			if (!fg_ParseScaled(Text.f_Right(Text.f_GetLen() - umint(iAt) - 1), gc_SendWindowLatencyUnits, 0.001, LatencySeconds))
			{
				o_Error = "Send window latency must be a number in us, ms or s, like 10gbit@10ms";

				return false;
			}

			// Twice the bandwidth-delay product: a send holds its bytes until the last of them is
			// acknowledged, and the acknowledgements come per pair of segments or on the timer
			Bytes = BitsPerSecond / 8.0 * LatencySeconds * 2.0;
		}
		else if (!fg_ParseScaled(Text, gc_SendWindowByteUnits, 1.0, Bytes))
		{
			o_Error = "Send window must be a byte count with an optional K, M, G, KiB, MiB, GiB, KB, MB or GB suffix, <rate>@<latency> like 10gbit@10ms, or default";

			return false;
		}

		if (Bytes < 1.0)
		{
			o_Bytes = 0;

			return true;
		}

		if (Bytes > fp64(uint64(1) << 40))
		{
			o_Error = "Send window is larger than a terabyte";

			return false;
		}

		o_Bytes = uint64(Bytes.f_ToIntRound());

		return true;
	}

	NStr::CStr fg_FormatSendWindow(uint64 _Bytes)
	{
		if (!_Bytes)
			return "default";
		if (!(_Bytes % (1024 * 1024)))
			return NStr::CStr::CFormat("{} MiB") << (_Bytes / (1024 * 1024));
		if (!(_Bytes % 1024))
			return NStr::CStr::CFormat("{} KiB") << (_Bytes / 1024);

		return NStr::CStr::CFormat("{} B") << _Bytes;
	}

	NStr::CStr fg_ValidateAuthenticatedUnixAddress(NStr::CStr const &_Scheme, NStr::CStr const &_Host)
	{
		if (NStr::fg_StrCmpNoCase(_Scheme, "wsa") != 0)
			return {};

		if (!NNetwork::fg_IsAuthenticatedUnixSupported())
			return "wsa requires kernel peer-process authentication (macOS/Linux); use the wss/TLS transport";

		// The authenticated unix handshake does not encrypt, so only allow it where the kernel keeps the
		// stream private
		if (!NNetwork::fg_IsUnixSocketAddressString(_Host))
			return "wsa connections require a unix socket address";

		return {};
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
