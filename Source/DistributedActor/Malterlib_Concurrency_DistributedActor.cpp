// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Core/Core>
#include <Mib/Process/Platform>
#include <Mib/Cryptography/RandomID>
#include <Mib/Cryptography/Certificate>
#include <Mib/Encoding/EJson>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Storage/Optional>

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		struct CSubSystem_Concurrency_DistributedActorDefaultManager : public CSubSystem
		{
			CSubSystem_Concurrency_DistributedActorDefaultManager();
			CSubSystem_Concurrency_DistributedActorDefaultManager(CActorDistributionManagerInitSettings const &_Settings);
			~CSubSystem_Concurrency_DistributedActorDefaultManager();
			void f_PreDestroyThreadSpecific() override;

			CActorDistributionManagerInitSettings m_DistributionManagerSettings;
			TCActor<CActorDistributionManager> m_DistributionManager;
		};

		CSubSystem_Concurrency_DistributedActorDefaultManager::CSubSystem_Concurrency_DistributedActorDefaultManager()
			: m_DistributionManagerSettings{{}, {}}
		{
			DMibError("You need to call fg_InitDistributionManager before using distributed actors");
		}

		CSubSystem_Concurrency_DistributedActorDefaultManager::CSubSystem_Concurrency_DistributedActorDefaultManager(CActorDistributionManagerInitSettings const &_Settings)
			: m_DistributionManagerSettings(_Settings)
		{
			fg_ConcurrencyManager(); // Add dependency to subsystem
			fg_RuntimeTypeRegistry();

			m_DistributionManager = fg_ConstructActor<CActorDistributionManager>(_Settings);
		}

		CSubSystem_Concurrency_DistributedActorDefaultManager::~CSubSystem_Concurrency_DistributedActorDefaultManager()
		{
			if (m_DistributionManager)
			{
				m_DistributionManager->f_BlockDestroy();
				m_DistributionManager.f_Clear();
			}
		}

		void CSubSystem_Concurrency_DistributedActorDefaultManager::f_PreDestroyThreadSpecific()
		{
			if (m_DistributionManager)
			{
				m_DistributionManager->f_BlockDestroy();
				m_DistributionManager.f_Clear();
			}
		}

		CSubSystem_Concurrency_DistributedActor::CSubSystem_Concurrency_DistributedActor()
		{
			fg_ConcurrencyManager(); // Add dependency to subsystem

			NCryptography::CCertificate::fs_RegisterExtension
				(
					"1.3.6.1.4.1.47722.1.1"
					, "MalterlibHostID"
					, "Malterlib Host ID"
				)
			;
		}

		CSubSystem_Concurrency_DistributedActor::~CSubSystem_Concurrency_DistributedActor()
		{
		}

		constinit TCSubSystem<CSubSystem_Concurrency_DistributedActorDefaultManager, ESubSystemDestruction_BeforeMemoryManager>
			g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager = {DAggregateInit}
		;

		constinit TCSubSystem<CSubSystem_Concurrency_DistributedActor, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_DistributedActor = {DAggregateInit};

		CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem()
		{
			return *g_MalterlibSubSystem_Concurrency_DistributedActor;
		}

		TCFuture<CDistributedActorPublication> fg_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, NPrivate::CDistributedActorInterfaceInfo &&_InterfaceInfo
				, NStr::CStr const &_Namespace
				, fp32 _WaitForPublicationsTimeout
			)
		{
			NPrivate::CDistributedActorData *pDistributedData = (NPrivate::CDistributedActorData *)_Actor->f_GetDistributedActorData().f_Get();
			TCActor<CActorDistributionManager> DistributionManager;

			if (!pDistributedData)
				return DMibErrorInstance("Not a distributed actor");
			else if (pDistributedData->m_bRemote)
				return DMibErrorInstance("You cannot publish a remote actor");
			else
			{
				DistributionManager = pDistributedData->m_DistributionManager.f_Lock();
				if (!DistributionManager)
					return DMibErrorInstance("Distribution manager has been deleted");
			}

			return DistributionManager
				(
					&CActorDistributionManager::fp_PublishActor
					, fg_Move(_Actor)
					, _Namespace
					, fg_Move(_InterfaceInfo)
					, _WaitForPublicationsTimeout
				)
			;
		}
	}

	CActorDistributionManagerHolder::CActorDistributionManagerHolder
		(
			CConcurrencyManager *_pConcurrencyManager
			, bool _bImmediateDelete
			, EPriority _Priority
			, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
		)
		: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
	{
	}

	NCryptography::CPublicKeySetting CActorDistributionCryptographySettings::fs_DefaultKeySetting()
	{
		return NCryptography::CPublicKeySettings_EC_secp521r1{};
	}

	void fg_InitDistributedActorSystem()
	{
		NPrivate::fg_DistributedActorSubSystem();
	}

	CCallingHostInfo const &CActorDistributionManager::fs_GetCallingHostInfo()
	{
		return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CallingHostInfo;
	}

	CCallingHostInfoScope::CCallingHostInfoScope(CCallingHostInfo &&_NewInfo, bool _bAddToCoroutine)
		: CCrossActorCallStateScope(_bAddToCoroutine)
		, mp_NewInfo(_NewInfo)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevInfo = fg_Move(ThreadLocal.m_CallingHostInfo);
		mp_pPrevScope = ThreadLocal.m_pCurrentCallingHostInfoScope;
		ThreadLocal.m_CallingHostInfo = fg_Move(_NewInfo);
		ThreadLocal.m_pCurrentCallingHostInfoScope = this;
	}

	CCallingHostInfoScope::~CCallingHostInfoScope()
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CallingHostInfo = fg_Move(mp_PrevInfo);
		ThreadLocal.m_pCurrentCallingHostInfoScope = mp_pPrevScope;
	}

	void CCallingHostInfoScope::f_Suspend() noexcept
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CallingHostInfo = fg_Move(mp_PrevInfo);
		ThreadLocal.m_pCurrentCallingHostInfoScope = mp_pPrevScope;
		CCrossActorCallStateScope::f_Suspend();
	}

	void CCallingHostInfoScope::f_ResumeNoExcept() noexcept
	{
		CCrossActorCallStateScope::f_ResumeNoExcept();
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevInfo = fg_Move(ThreadLocal.m_CallingHostInfo);
		mp_pPrevScope = ThreadLocal.m_pCurrentCallingHostInfoScope;
		ThreadLocal.m_CallingHostInfo = fg_Move(mp_NewInfo);
		ThreadLocal.m_pCurrentCallingHostInfoScope = this;
	}

	void CCallingHostInfoScope::f_InitialSuspend()
	{
	}

	NFunction::TCFunctionMovable<void () noexcept> CCallingHostInfoScope::f_StoreState(bool _bFromSuspend)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		if (ThreadLocal.m_pCurrentCallingHostInfoScope != this)
			return nullptr;

		struct CState
		{
			CState(CCallingHostInfo const &_CallingHostInfo)
				: m_NewInfo(_CallingHostInfo)
			{
			}

			CState(CState &&_Other)
				: m_NewInfo(fg_Move(_Other.m_NewInfo))
			{
				DMibFastCheck(!_Other.m_Scope);
			}

			CState(CState const &) = delete;

			~CState()
			{
				if (!m_Scope)
					return;

				DMibFastCheck(m_StoredThreadID == NSys::fg_Thread_GetCurrentUID());
				m_Scope.f_Clear();
			}

			CCallingHostInfo m_NewInfo;
			NStorage::TCOptional<CCallingHostInfoScope> m_Scope;
#if DMibEnableSafeCheck > 0
			mint m_StoredThreadID = 0;
#endif
		};

		return [State = CState{ThreadLocal.m_CallingHostInfo}]() mutable noexcept
			{
				DMibFastCheck(!State.m_Scope);
#if DMibEnableSafeCheck > 0
				State.m_StoredThreadID = NSys::fg_Thread_GetCurrentUID();
#endif
				State.m_Scope = fg_Construct(fg_Move(State.m_NewInfo), false);
			}
		;
	}

	CAuthenticationHandlerIDScope::CAuthenticationHandlerIDScope(uint32 _HandlerID, bool _bAddToCoroutine)
		: CCrossActorCallStateScope(_bAddToCoroutine)
		, mp_HandlerID(_HandlerID)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevHandlerID = ThreadLocal.m_CurrentAuthenticationHandlerID;
		mp_pPrevScope = ThreadLocal.m_pCurrentAuthenticationHandlerIDScope;
		ThreadLocal.m_CurrentAuthenticationHandlerID = _HandlerID;
		ThreadLocal.m_pCurrentAuthenticationHandlerIDScope = this;
	}

	CAuthenticationHandlerIDScope::~CAuthenticationHandlerIDScope()
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CurrentAuthenticationHandlerID = mp_PrevHandlerID;
		ThreadLocal.m_pCurrentAuthenticationHandlerIDScope = mp_pPrevScope;
	}

	void CAuthenticationHandlerIDScope::f_Suspend() noexcept
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CurrentAuthenticationHandlerID = mp_PrevHandlerID;
		ThreadLocal.m_pCurrentAuthenticationHandlerIDScope = mp_pPrevScope;
		CCrossActorCallStateScope::f_Suspend();
	}

	void CAuthenticationHandlerIDScope::f_ResumeNoExcept() noexcept
	{
		CCrossActorCallStateScope::f_ResumeNoExcept();
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevHandlerID = ThreadLocal.m_CurrentAuthenticationHandlerID;
		mp_pPrevScope = ThreadLocal.m_pCurrentAuthenticationHandlerIDScope;
		ThreadLocal.m_CurrentAuthenticationHandlerID = mp_HandlerID;
		ThreadLocal.m_pCurrentAuthenticationHandlerIDScope = this;
	}

	void CAuthenticationHandlerIDScope::f_InitialSuspend()
	{
	}

	NFunction::TCFunctionMovable<void () noexcept> CAuthenticationHandlerIDScope::f_StoreState(bool _bFromSuspend)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		if (ThreadLocal.m_pCurrentAuthenticationHandlerIDScope != this)
			return nullptr;

		struct CState
		{
			CState(uint32 _HandlerID)
				: m_HandlerID(_HandlerID)
			{
			}

			CState(CState &&_Other)
				: m_HandlerID(_Other.m_HandlerID)
			{
				DMibFastCheck(!_Other.m_Scope);
			}

			CState(CState const &) = delete;

			~CState()
			{
				if (!m_Scope)
					return;

				DMibFastCheck(m_StoredThreadID == NSys::fg_Thread_GetCurrentUID());
				m_Scope.f_Clear();
			}

			uint32 m_HandlerID;
			NStorage::TCOptional<CAuthenticationHandlerIDScope> m_Scope;
#if DMibEnableSafeCheck > 0
			mint m_StoredThreadID = 0;
#endif
		};

		return [State = CState{ThreadLocal.m_CurrentAuthenticationHandlerID}]() mutable noexcept
			{
				DMibFastCheck(!State.m_Scope);
#if DMibEnableSafeCheck > 0
				State.m_StoredThreadID = NSys::fg_Thread_GetCurrentUID();
#endif
				State.m_Scope = fg_Construct(State.m_HandlerID, false);
			}
		;
	}

	uint32 CAuthenticationHandlerIDScope::fs_GetCurrentHandlerID()
	{
		return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CurrentAuthenticationHandlerID;
	}

	CPriorityScope::CPriorityScope(uint8 _Priority, bool _bAddToCoroutine)
		: CCrossActorCallStateScope(_bAddToCoroutine)
		, mp_Priority(_Priority)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevPriority = ThreadLocal.m_CurrentPriority;
		mp_pPrevScope = ThreadLocal.m_pCurrentPriorityScope;
		ThreadLocal.m_CurrentPriority = _Priority;
		ThreadLocal.m_pCurrentPriorityScope = this;
	}

	CPriorityScope::~CPriorityScope()
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CurrentPriority = mp_PrevPriority;
		ThreadLocal.m_pCurrentPriorityScope = mp_pPrevScope;
	}

	void CPriorityScope::f_Suspend() noexcept
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		ThreadLocal.m_CurrentPriority = mp_PrevPriority;
		ThreadLocal.m_pCurrentPriorityScope = mp_pPrevScope;
		CCrossActorCallStateScope::f_Suspend();
	}

	void CPriorityScope::f_ResumeNoExcept() noexcept
	{
		CCrossActorCallStateScope::f_ResumeNoExcept();
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		mp_PrevPriority = ThreadLocal.m_CurrentPriority;
		mp_pPrevScope = ThreadLocal.m_pCurrentPriorityScope;
		ThreadLocal.m_CurrentPriority = mp_Priority;
		ThreadLocal.m_pCurrentPriorityScope = this;
	}

	void CPriorityScope::f_InitialSuspend()
	{
	}

	NFunction::TCFunctionMovable<void () noexcept> CPriorityScope::f_StoreState(bool _bFromSuspend)
	{
		auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
		if (ThreadLocal.m_pCurrentPriorityScope != this)
			return nullptr;

		struct CState
		{
			CState(uint8 _Priority)
				: m_Priority(_Priority)
			{
			}
			uint8 m_Priority;
			NStorage::TCUniquePointer<CPriorityScope> m_Scope;
#if DMibEnableSafeCheck > 0
			mint m_StoredThreadID;
#endif
		};

		NStorage::TCSharedPointer<CState> pState = fg_Construct(mp_Priority);

		return
			[pState]() noexcept
			{
				auto &State = *pState;
				if (State.m_Scope)
				{
					DMibFastCheck(State.m_StoredThreadID == NSys::fg_Thread_GetCurrentUID());
					State.m_Scope.f_Clear();
					return;
				}
				DMibFastCheck(!State.m_Scope);
#if DMibEnableSafeCheck > 0
				State.m_StoredThreadID = NSys::fg_Thread_GetCurrentUID();
#endif
				State.m_Scope = fg_Construct(State.m_Priority, false);
			}
		;
	}

	uint8 CPriorityScope::fs_GetCurrentPriority()
	{
		return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CurrentPriority;
	}

	CCallingHostInfo const &fg_GetCallingHostInfo()
	{
		return CActorDistributionManager::fs_GetCallingHostInfo();
	}

	NStr::CStr const &fg_GetCallingHostID()
	{
		return CActorDistributionManager::fs_GetCallingHostInfo().f_GetRealHostID();
	}

	CActorDistributionManagerInitSettings fg_InitDistributionManager(CActorDistributionManagerInitSettings const &_Settings)
	{
		if (NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager.f_WasCreated())
			return NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager->m_DistributionManagerSettings;
		NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager.f_Construct
			(
				[&](void *_pMemory) -> NPrivate::CSubSystem_Concurrency_DistributedActorDefaultManager *
				{
					return new(_pMemory) NPrivate::CSubSystem_Concurrency_DistributedActorDefaultManager(_Settings);
				}
			)
		;
		return _Settings;
	}

	TCActor<CActorDistributionManager> const &fg_GetDistributionManager()
	{
		return NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager->m_DistributionManager;
	}

	CActorDistributionCryptographySettings::CActorDistributionCryptographySettings(NStr::CStr const &_HostID)
		: m_HostID(_HostID)
	{
	}

	CActorDistributionCryptographySettings::~CActorDistributionCryptographySettings()
	{
	}

	void CActorDistributionCryptographySettings::f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, NCryptography::CPublicKeySetting _KeySetting)
	{
		NPrivate::fg_DistributedActorSubSystem(); // Register extension if needed

		m_Subject = fg_Format("Malterlib Distributed Actors - {}", NCryptography::fg_RandomID());

		NCryptography::CCertificateOptions Options;
		Options.m_KeySetting = _KeySetting;
		Options.m_Hostnames = _HostNames;
		Options.m_CommonName = m_Subject;
		auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
		Extension.m_bCritical = false;
		Extension.m_Value = m_HostID;

		NCryptography::CCertificateSignOptions SignOptions;
		SignOptions.m_Serial = 1;
		SignOptions.m_Days = 10*365;

		NCryptography::CCertificate::fs_GenerateSelfSignedCertAndKey
			(
				Options
				, m_PublicCertificate
				, m_PrivateKey
				, SignOptions
			)
		;

		// These are now all invalid
		m_RemoteClientCertificates.f_Clear();
		m_SignedClientCertificates.f_Clear();
	}

	NContainer::CByteVector CActorDistributionCryptographySettings::f_GenerateRequest() const
	{
		NPrivate::fg_DistributedActorSubSystem(); // Register extension if needed

		NContainer::CByteVector Return;
		NContainer::CSecureByteVector KeyData = m_PrivateKey;

		NCryptography::CCertificateOptions Options;
		Options.m_CommonName = m_Subject;
		auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
		Extension.m_bCritical = false;
		Extension.m_Value = m_HostID;

		NCryptography::CCertificate::fs_GenerateClientCertificateRequest
			(
				Options
				, Return
				, KeyData
			)
		;
		return Return;
	}

	NContainer::CByteVector CActorDistributionCryptographySettings::f_SignRequest(NContainer::CByteVector const &_Request)
	{
		NCryptography::CCertificateSignOptions SignOptions;
		SignOptions.m_Serial = ++m_Serial;
		SignOptions.m_Days = 10*365;

		NContainer::CByteVector Return;
		NCryptography::CCertificate::fs_SignClientCertificate
			(
				m_PublicCertificate
				, m_PrivateKey
				, _Request
				, Return
				, SignOptions
			)
		;
		return Return;
	}

	void CActorDistributionCryptographySettings::f_AddRemoteServer
		(
			NWeb::NHTTP::CURL const &_URL
			, NContainer::CByteVector const &_ServerCert
			, NContainer::CByteVector const &_ClientCert
		)
	{
		auto &NewRemote = m_RemoteClientCertificates[_URL];
		NewRemote.m_PublicServerCertificate = _ServerCert;
		NewRemote.m_PublicClientCertificate = _ClientCert;
	}

	NStr::CStr CActorDistributionCryptographySettings::f_GetHostID() const
	{
		return NCryptography::CCertificate::fs_GetCertificateFingerprint(m_PublicCertificate);
	}

	CActorDistributionConnectionSettings::CActorDistributionConnectionSettings()
	{
	}

	CActorDistributionConnectionSettings::~CActorDistributionConnectionSettings()
	{
	}

	void CActorDistributionConnectionSettings::f_SetCryptography(CActorDistributionCryptographySettings const &_Settings)
	{
		auto pRemote = _Settings.m_RemoteClientCertificates.f_FindEqual(m_ServerURL);
		if (pRemote)
		{
			m_PrivateClientKey = _Settings.m_PrivateKey;
			m_PublicClientCertificate = pRemote->m_PublicClientCertificate;
			m_PublicServerCertificate = pRemote->m_PublicServerCertificate;
		}
	}

	CActorDistributionListenSettings::CActorDistributionListenSettings(uint16 _Port, NNetwork::ENetAddressType _AddressType)
	{
		if (_AddressType == NNetwork::ENetAddressType_None || _AddressType == NNetwork::ENetAddressType_TCPv4)
		{
			NWeb::NHTTP::CURL URL{NStr::fg_Format("wss://[IPv4:0.0.0.0]:{}/", _Port)};
			m_ListenAddresses.f_Insert(URL);
		}
		if (_AddressType == NNetwork::ENetAddressType_None || _AddressType == NNetwork::ENetAddressType_TCPv6)
		{
			NWeb::NHTTP::CURL URL{NStr::fg_Format("wss://[IPv6:::]:{}/", _Port)};
			m_ListenAddresses.f_Insert(URL);
		}
	}

	CActorDistributionListenSettings::~CActorDistributionListenSettings()
	{
	}

	CActorDistributionListenSettings::CActorDistributionListenSettings(NContainer::TCVector<NWeb::NHTTP::CURL> const &_Addresses)
		: m_ListenAddresses(_Addresses)
	{
	}

	void CActorDistributionListenSettings::f_SetCryptography(CActorDistributionCryptographySettings const &_Settings)
	{
		m_PublicCertificate = _Settings.m_PublicCertificate;
		m_CACertificate = _Settings.m_PublicCertificate;
		m_PrivateKey = _Settings.m_PrivateKey;
	}

	CCallingHostInfo::CCallingHostInfo()
	{
	}

	CCallingHostInfo::CCallingHostInfo
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, TCDistributedActor<ICDistributedActorAuthenticationHandler> const &_AuthenticationHandler
			, NStr::CStr const &_UniqueHostID
			, CHostInfo const &_HostInfo
			, NStr::CStr const &_LastExecutionID
			, uint32 _ProtocolVersion
			, NStr::CStr const &_ClaimedUserID
			, NStr::CStr const &_ClaimedUserName
			, NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
			, uint8 _CallPriority
		)
		: mp_DistributionManager(_DistributionManager)
		, mp_AuthenticationHandler(_AuthenticationHandler)
		, mp_UniqueHostID(_UniqueHostID)
		, mp_HostInfo(_HostInfo)
		, mp_LastExecutionID(_LastExecutionID)
		, mp_ProtocolVersion(_ProtocolVersion)
		, mp_ClaimedUserID(_ClaimedUserID)
		, mp_ClaimedUserName(_ClaimedUserName)
		, mp_pHost(_pHost)
		, mp_CallPriority(_CallPriority)
	{
	}

	uint32 CCallingHostInfo::f_GetProtocolVersion() const
	{
		return mp_ProtocolVersion;
	}

	NStr::CStr const &CCallingHostInfo::f_GetRealHostID() const
	{
		return mp_HostInfo.m_HostID;
	}

	NStr::CStr const &CCallingHostInfo::f_GetUniqueHostID() const
	{
		return mp_UniqueHostID;
	}

	CHostInfo const &CCallingHostInfo::f_GetHostInfo() const
	{
		return mp_HostInfo;
	}

	NStr::CStr const &CCallingHostInfo::f_LastExecutionID() const
	{
		return mp_LastExecutionID;
	}

	NStr::CStr const &CCallingHostInfo::f_GetClaimedUserID() const
	{
		return mp_ClaimedUserID;
	}

	NStr::CStr const &CCallingHostInfo::f_GetClaimedUserName() const
	{
		return mp_ClaimedUserName;
	}

	bool CCallingHostInfo::operator == (CCallingHostInfo const &_Right) const noexcept
	{
		return NStorage::fg_TupleReferences(mp_UniqueHostID, mp_HostInfo.m_HostID, mp_LastExecutionID)
			== NStorage::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_HostInfo.m_HostID, _Right.mp_LastExecutionID)
		;
	}

	COrdering_Strong CCallingHostInfo::operator <=> (CCallingHostInfo const &_Right) const noexcept
	{
		return NStorage::fg_TupleReferences(mp_UniqueHostID, mp_HostInfo.m_HostID, mp_LastExecutionID)
			<=> NStorage::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_HostInfo.m_HostID, _Right.mp_LastExecutionID)
		;
	}

	TCActor<CActorDistributionManager> CCallingHostInfo::f_GetDistributionManager() const
	{
		return mp_DistributionManager.f_Lock();
	}

	TCDistributedActor<ICDistributedActorAuthenticationHandler> CCallingHostInfo::f_GetAuthenticationHandler() const
	{
		return mp_AuthenticationHandler.f_Lock();
	}

	NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> CCallingHostInfo::f_GetHost() const
	{
		return mp_pHost.f_Lock();
	}

	uint8 CCallingHostInfo::f_GetCallPriority() const
	{
		return mp_CallPriority;
	}

	TCFuture<CActorSubscription> CCallingHostInfo::f_OnDisconnect(TCActorFunctorWeak<TCFuture<void> ()> _fOnDisconnect) const
	{
		auto DistributionManager = mp_DistributionManager.f_Lock();
		if (!DistributionManager)
			return DMibErrorInstance("Distribution manager was deleted");

		return DistributionManager(&CActorDistributionManager::fp_OnRemoteDisconnect, fg_Move(_fOnDisconnect), mp_UniqueHostID, mp_LastExecutionID);
	}

	CHostInfo::CHostInfo()
	{
	}

	CHostInfo::~CHostInfo()
	{
	}

	CHostInfo::CHostInfo(NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
		: m_HostID(_HostID)
		, m_FriendlyName(_FriendlyName)
	{
	}

	void CHostInfo::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		// Warning: This is used in actor interfaces, if changed you need to handle this
		_Stream << m_HostID;
		_Stream << m_FriendlyName;
	}

	void CHostInfo::f_Consume(CDistributedActorReadStream &_Stream)
	{
		// Warning: This is used in actor interfaces, if changed you need to handle this
		_Stream >> m_HostID;
		_Stream >> m_FriendlyName;
	}

	bool CHostInfo::f_IsEmpty() const
	{
		return m_FriendlyName.f_IsEmpty() && m_HostID.f_IsEmpty();
	}

	NStr::CStr CHostInfo::f_GetDesc() const
	{
		if (m_FriendlyName.f_IsEmpty())
			return m_HostID;
		else if (m_HostID.f_IsEmpty())
			return m_FriendlyName;
		return fg_Format("{} [{}]", m_HostID, m_FriendlyName);
	}

	NStr::CStr CHostInfo::fs_FormatFriendlyNameForTable(NStr::CStr const &_FriendlyName, NCommandLine::CAnsiEncoding const &_AnsiEncoding)
	{
		using namespace NStr;

		CStr UserName;
		CStr HostName;
		CStr Application;
		aint nParsed;
		(CStr::CParse("{}@{}/{}") >> UserName >> HostName >> Application).f_Parse(_FriendlyName, nParsed);

		CStr Return;
		if (nParsed == 3)
		{
			return "{}{}{} {}{}{} ({}{}{})"_f
				<< _AnsiEncoding.f_Foreground256(75)
				<< HostName
				<< _AnsiEncoding.f_Default()
				<< _AnsiEncoding.f_Foreground256(246)
				<< Application
				<< _AnsiEncoding.f_Default()
				<< _AnsiEncoding.f_Foreground256(75)
				<< UserName
				<< _AnsiEncoding.f_Default()
			;
		}
		else
			return _FriendlyName;
	}

	NStr::CStr CHostInfo::f_GetDescColored(NCommandLine::EAnsiEncodingFlag _AnsiFlags) const
	{
		using namespace NStr;

		CStr Return;

		if (m_HostID.f_IsEmpty())
			Return += "Invalid";
		else
			Return += m_HostID;

		if (m_FriendlyName.f_IsEmpty())
			return Return;

		Return += " ";

		CStr UserName;
		CStr HostName;
		aint nParsed;
		(CStr::CParse("{}@{}") >> UserName >> HostName).f_Parse(m_FriendlyName, nParsed);

		NCommandLine::CAnsiEncoding AnsiEncoding(_AnsiFlags);

		if (nParsed == 2)
		{
			Return += "[{}{}{}@{}{}{}]"_f
				<< AnsiEncoding.f_Foreground256(75)
				<< UserName
				<< AnsiEncoding.f_Foreground256(246)
				<< AnsiEncoding.f_Foreground256(75)
				<< HostName
				<< AnsiEncoding.f_Default()
			;
		}
		else
			Return += "[{}]"_f << m_FriendlyName;

		return Return;
	}

	NPrivate::CDistributedActorInterfaceInfo::CDistributedActorInterfaceInfo() = default;

	CDistributedActorProtocolVersions::CDistributedActorProtocolVersions() = default;
	CDistributedActorProtocolVersions::CDistributedActorProtocolVersions(uint32 _MinSupported, uint32 _MaxSupported)
		: m_MinSupported(_MinSupported)
		, m_MaxSupported(_MaxSupported)
	{
	}

	bool CDistributedActorProtocolVersions::f_HighestSupportedVersion(CDistributedActorProtocolVersions const &_Other, uint32 &o_Version) const
	{
		uint32 Version = fg_Min(m_MaxSupported, _Other.m_MaxSupported);
		bool bSuccess =
			Version >= m_MinSupported
			&& Version <= m_MaxSupported
			&& Version >= _Other.m_MinSupported
			&& Version <= _Other.m_MaxSupported
		;
		if (bSuccess)
			o_Version = Version;
		return bSuccess;
	}

	bool CDistributedActorProtocolVersions::f_ValidVersion(uint32 _Version)
	{
		return _Version >= m_MinSupported && _Version <= m_MaxSupported;
	}

	CDistributedActorIdentifier::CDistributedActorIdentifier()
	{

	}

	CDistributedActorIdentifier::CDistributedActorIdentifier(NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost, NStr::CStr const &_ActorID)
		: mp_pHost(_pHost)
		, mp_ActorID(_ActorID)
	{
	}

	bool CDistributedActorIdentifier::operator == (TCActor<> const &_Right) const noexcept
	{
		if (!_Right)
			return false;

		auto const &pRightData = _Right->f_GetDistributedActorData();
		if (!pRightData)
			return false;

		NPrivate::CDistributedActorData const *pRightInternal = (NPrivate::CDistributedActorData const *)pRightData.f_Get();

		return NStorage::fg_TupleReferences(mp_pHost, mp_ActorID) == NStorage::fg_TupleReferences(pRightInternal->m_pHost, pRightInternal->m_ActorID);
	}

	COrdering_Weak CDistributedActorIdentifier::operator <=> (TCActor<> const &_Right) const noexcept
	{
		if (!_Right)
			return COrdering_Weak::greater;

		auto const &pRightData = _Right->f_GetDistributedActorData();
		if (!pRightData)
			return COrdering_Weak::greater;

		NPrivate::CDistributedActorData const *pRightInternal = (NPrivate::CDistributedActorData const *)pRightData.f_Get();

		return NStorage::fg_TupleReferences(mp_pHost, mp_ActorID) <=> NStorage::fg_TupleReferences(pRightInternal->m_pHost, pRightInternal->m_ActorID);
	}

	NPrivate::ICHost::~ICHost()
	{
	}

	NPrivate::ICHost::ICHost(CDistributedActorHostInfo &&_Info)
		: m_HostInfo(fg_Move(_Info))
	{
	}

	bool CActorDistributionManager::fs_IsValidNamespaceName(NStr::CStr const &_String)
	{
		return NNetwork::fg_IsValidHostname(_String, "/");
	}

	bool CActorDistributionManager::fs_IsValidHostID(NStr::CStr const &_String)
	{
		return NNetwork::fg_IsValidHostname(_String);
	}

	bool CActorDistributionManager::fs_IsValidUserID(NStr::CStr const &_String)
	{
		return NNetwork::fg_IsValidHostname(_String);
	}

	CDistributedActorInterfaceShare::CDistributedActorInterfaceShare()
	{
	}

	CDistributedActorInterfaceShare::~CDistributedActorInterfaceShare()
	{
	}

	CDistributedActorInterfaceShare::CDistributedActorInterfaceShare
		(
			NContainer::TCVector<uint32> const &_Hierarchy
			, CDistributedActorProtocolVersions const &_ProtocolVersions
			, TCDistributedActor<CActor> &&_Actor
		)
		: m_Hierarchy(_Hierarchy)
		, m_ProtocolVersions(_ProtocolVersions)
		, m_Actor(fg_Move(_Actor))
	{
	}

#ifndef DCompiler_MSVC_Workaround
	template TCDistributedActor<CActor> fg_ConstructRemoteDistributedActor<CActor>
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, NStr::CStr const &_ActorID
			, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
			, uint32 _ProtocolVersion
		)
	;

	template class TCActor<CActorDistributionManager>;

	template auto TCActor<CActorDistributionManager>::f_InternalCallActor
		<
			&CActorDistributionManager::f_CallRemote
			, EVirtualCall::mc_NotVirtual
			, NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CIOByteVector &&
			, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext &
		>
		(
			NMib::NStorage::TCSharedPointer<NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CIOByteVector &&
			, NPrivate::CDistributedActorStreamContext &
		) const &
		-> TCFuture<NContainer::CIOByteVector>
	;

	template auto TCActor<CActorDistributionManager>::f_InternalCallActor
		<
			&CActorDistributionManager::f_CallRemote
			, EVirtualCall::mc_NotVirtual
			, NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CIOByteVector &&
			, NMib::NConcurrency::NPrivate::CDistributedActorStreamContext &
		>
		(
			NMib::NStorage::TCSharedPointer<NPrivate::CDistributedActorData> &&
			, NMib::NContainer::CIOByteVector &&
			, NPrivate::CDistributedActorStreamContext &
		) &&
		-> TCFuture<NContainer::CIOByteVector>
	;
#endif
}
