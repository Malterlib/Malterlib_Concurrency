// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Process/Platform>
#include <Mib/Cryptography/RandomID>

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			struct CSubSystem_Concurrency_DistributedActorDefaultManager : public CSubSystem
			{
				CSubSystem_Concurrency_DistributedActorDefaultManager();
				CSubSystem_Concurrency_DistributedActorDefaultManager(CActorDistributionManagerInitSettings const &_Settings);
				~CSubSystem_Concurrency_DistributedActorDefaultManager();
				void f_DestroyThreadSpecific() override;

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
			
			void CSubSystem_Concurrency_DistributedActorDefaultManager::f_DestroyThreadSpecific()
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
				
				NNet::CSSLContext::fs_RegisterExtension
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
			
			TCSubSystem<CSubSystem_Concurrency_DistributedActorDefaultManager, ESubSystemDestruction_BeforeMemoryManager> 
				g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager = {DAggregateInit}
			;
			
			TCSubSystem<CSubSystem_Concurrency_DistributedActor, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_DistributedActor = {DAggregateInit};
			
			CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem()
			{
				return *g_MalterlibSubSystem_Concurrency_DistributedActor;
			}
			
			template <>
			void fg_CopyReplyToContinuation(TCContinuation<void> &_Continuation, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data, CDistributedActorStreamContextSending &_Context, uint32 _Version)
			{
				NStream::CBinaryStreamMemoryPtr<> ReplyStream;
				ReplyStream.f_OpenRead(_Data);
				if (fg_CopyReplyToContinuationOrAsyncResultShared(ReplyStream, _Continuation))
					return;
				if (!_Context.f_ValueReceived())
				{
					_Continuation.f_SetException(DMibErrorInstance("Invalid set of parameter and return types"));
					return;
				}
				_Continuation.f_SetResult();
			}
		}
		
		NNet::CSSLKeySetting CActorDistributionCryptographySettings::fs_DefaultKeySetting()
		{
			return NNet::CSSLKeySettings_EC_secp521r1{};
		}
		
		void fg_InitDistributedActorSystem()
		{
			NPrivate::fg_DistributedActorSubSystem();
		}
		
		NStr::CStr &fg_DistributedActorSuggestedEnclave()
		{
			return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_SuggestedEnclave;
		}

		CCallingHostInfo const &CActorDistributionManager::fs_GetCallingHostInfo()
		{
			return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CallingHostInfo;
		}
		
		CCallingHostInfo const &fg_GetCallingHostInfo()
		{
			return CActorDistributionManager::fs_GetCallingHostInfo();
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
		
		void CActorDistributionCryptographySettings::f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, NNet::CSSLKeySetting _KeySetting)
		{
			NPrivate::fg_DistributedActorSubSystem(); // Register extension if needed

			m_Subject = fg_Format("Malterlib Distributed Actors - {}", NCryptography::fg_RandomID());
			
			NNet::CSSLContext::CCertificateOptions Options;
			Options.m_KeySetting = _KeySetting;
			Options.m_Hostnames = _HostNames;
			Options.m_Subject = m_Subject; 
			auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
			Extension.m_bCritical = false; 
			Extension.m_Value = m_HostID;
			
			NNet::CSignOptions SignOptions;
			SignOptions.m_Serial = 1;
			SignOptions.m_Days = 10*365;
			
			NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey
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
		
		NContainer::TCVector<uint8> CActorDistributionCryptographySettings::f_GenerateRequest() const
		{
			NPrivate::fg_DistributedActorSubSystem(); // Register extension if needed
			
			NContainer::TCVector<uint8> Return;
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> KeyData = m_PrivateKey;
			
			NNet::CSSLContext::CCertificateOptions Options;
			Options.m_Subject = m_Subject; 
			auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
			Extension.m_bCritical = false; 
			Extension.m_Value = m_HostID; 
			
			NNet::CSSLContext::fs_GenerateClientCertificateRequest
				(
					Options
					, Return
					, KeyData
				)
			;
			return Return;
		}
		
		NContainer::TCVector<uint8> CActorDistributionCryptographySettings::f_SignRequest(NContainer::TCVector<uint8> const &_Request)
		{
			NNet::CSignOptions SignOptions;
			SignOptions.m_Serial = ++m_Serial;
			SignOptions.m_Days = 10*365;
			
			NContainer::TCVector<uint8> Return;
			NNet::CSSLContext::fs_SignClientCertificate
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
				NHTTP::CURL const &_URL
				, NContainer::TCVector<uint8> const &_ServerCert
				, NContainer::TCVector<uint8> const &_ClientCert
			)
		{
			auto &NewRemote = m_RemoteClientCertificates[_URL];
			NewRemote.m_PublicServerCertificate = _ServerCert;
			NewRemote.m_PublicClientCertificate = _ClientCert;
		}
		
		NStr::CStr CActorDistributionCryptographySettings::f_GetHostID() const
		{
			return NNet::CSSLContext::fs_GetCertificateFingerprint(m_PublicCertificate);
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
		
		CActorDistributionListenSettings::CActorDistributionListenSettings(uint16 _Port, NNet::ENetAddressType _AddressType)
		{
			if (_AddressType == NNet::ENetAddressType_None || _AddressType == NNet::ENetAddressType_TCPv4)
			{
				NHTTP::CURL URL{NStr::fg_Format("wss://[IPv4:0.0.0.0]:{}/", _Port)};
				m_ListenAddresses.f_Insert(URL);
			}
			if (_AddressType == NNet::ENetAddressType_None || _AddressType == NNet::ENetAddressType_TCPv6)
			{
				NHTTP::CURL URL{NStr::fg_Format("wss://[IPv6:::]:{}/", _Port)};
				m_ListenAddresses.f_Insert(URL);
			}
		}

		CActorDistributionListenSettings::~CActorDistributionListenSettings()
		{
		}
		
		CActorDistributionListenSettings::CActorDistributionListenSettings(NContainer::TCVector<NHTTP::CURL> const &_Addresses)
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
				, NStr::CStr const &_UniqueHostID
				, CHostInfo const &_HostInfo
				, NStr::CStr const &_LastExecutionID
				, uint32 _ProtocolVersion
			)
			: mp_DistributionManager(_DistributionManager)
			, mp_UniqueHostID(_UniqueHostID) 
			, mp_HostInfo(_HostInfo)
			, mp_LastExecutionID(_LastExecutionID)
			, mp_ProtocolVersion(_ProtocolVersion) 
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

		bool CCallingHostInfo::operator ==(CCallingHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(mp_UniqueHostID, mp_HostInfo.m_HostID, mp_LastExecutionID) 
				== NContainer::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_HostInfo.m_HostID, _Right.mp_LastExecutionID)
			; 
		}		

		bool CCallingHostInfo::operator <(CCallingHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(mp_UniqueHostID, mp_HostInfo.m_HostID, mp_LastExecutionID) 
				< NContainer::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_HostInfo.m_HostID, _Right.mp_LastExecutionID)
			; 
		}		
		
		TCActor<CActorDistributionManager> const &CCallingHostInfo::f_GetDistributionManager() const
		{
			return mp_DistributionManager;
		}
		
		TCDispatchedActorCall<CActorSubscription> CCallingHostInfo::f_OnDisconnect(TCActor<CActor> const &_Actor, NFunction::TCFunctionMutable<void ()> &&_fOnDisconnect) const
		{
			return fg_ConcurrentDispatch
				(
					[DistributionManager = mp_DistributionManager, fOnDisconnect = fg_Move(_fOnDisconnect), UniqueHostID = mp_UniqueHostID, LastExecutionID = mp_LastExecutionID, _Actor]() mutable
					{
						TCContinuation<CActorSubscription> Continuation;
						DistributionManager(&CActorDistributionManager::fp_OnRemoteDisconnect, _Actor, fg_Move(fOnDisconnect), UniqueHostID, LastExecutionID) > Continuation;
						return Continuation;
					}
				)
			;
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
		
		NStr::CStr CHostInfo::f_GetDesc() const
		{
			if (m_FriendlyName.f_IsEmpty())
				return m_HostID;
			else if (m_HostID.f_IsEmpty())
				return m_FriendlyName;
			return fg_Format("{} [{}]", m_HostID, m_FriendlyName);
		}
	
		bool CHostInfo::operator ==(CHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_HostID, m_FriendlyName) == NContainer::fg_TupleReferences(_Right.m_HostID, _Right.m_FriendlyName);
		}
		
		bool CHostInfo::operator <(CHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_HostID, m_FriendlyName) < NContainer::fg_TupleReferences(_Right.m_HostID, _Right.m_FriendlyName);
		}
		
		CDistributedActorInheritanceHeirarchyPublish::CDistributedActorInheritanceHeirarchyPublish()
		{
		}
	
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
		
		bool CDistributedActorProtocolVersions::operator == (CDistributedActorProtocolVersions const &_Other) const
		{
			return NContainer::fg_TupleReferences(m_MinSupported, m_MaxSupported) == NContainer::fg_TupleReferences(_Other.m_MinSupported, _Other.m_MaxSupported); 
		}
		
		bool CDistributedActorProtocolVersions::operator < (CDistributedActorProtocolVersions const &_Other) const
		{
			return NContainer::fg_TupleReferences(m_MinSupported, m_MaxSupported) < NContainer::fg_TupleReferences(_Other.m_MinSupported, _Other.m_MaxSupported); 
		}
		
		CDistributedActorIdentifier::CDistributedActorIdentifier()
		{
			
		}
		
		CDistributedActorIdentifier::CDistributedActorIdentifier(NPtr::TCWeakPointer<NPrivate::ICHost> const &_pHost, NStr::CStr const &_ActorID)
			: mp_pHost(_pHost)
			, mp_ActorID(_ActorID)
		{
		}
		
		bool CDistributedActorIdentifier::operator == (CDistributedActorIdentifier const &_Other) const
		{
			return NContainer::fg_TupleReferences(mp_pHost, mp_ActorID) == NContainer::fg_TupleReferences(_Other.mp_pHost, _Other.mp_ActorID); 
		}
		
		bool CDistributedActorIdentifier::operator < (CDistributedActorIdentifier const &_Other) const
		{
			return NContainer::fg_TupleReferences(mp_pHost, mp_ActorID) < NContainer::fg_TupleReferences(_Other.mp_pHost, _Other.mp_ActorID); 
		}
		
		bool operator == (CDistributedActorIdentifier const &_Left, TCActor<> const &_Right)
		{
			if (!_Right)
				return false;
			auto const &pRightData = _Right->f_GetDistributedActorData();
			if (!pRightData)
				return false;
			NActorDistributionManagerInternal::CDistributedActorDataInternal const *pRightInternal 
				= (NActorDistributionManagerInternal::CDistributedActorDataInternal const *)pRightData.f_Get()
			;
			return NContainer::fg_TupleReferences(_Left.mp_pHost, _Left.mp_ActorID) == NContainer::fg_TupleReferences(pRightInternal->m_pHost, pRightInternal->m_ActorID); 
		}
		
		bool operator == (TCActor<> const &_Left, CDistributedActorIdentifier const &_Right)
		{
			if (!_Left)
				return false;
			auto const &pLeftData = _Left->f_GetDistributedActorData();
			if (!pLeftData)
				return false;
			NActorDistributionManagerInternal::CDistributedActorDataInternal const *pLeftInternal 
				= (NActorDistributionManagerInternal::CDistributedActorDataInternal const *)pLeftData.f_Get()
			;
			return NContainer::fg_TupleReferences(pLeftInternal->m_pHost, pLeftInternal->m_ActorID) == NContainer::fg_TupleReferences(_Right.mp_pHost, _Right.mp_ActorID); 
		}
		
		NPrivate::ICHost::~ICHost()
		{
		}
		
		bool CActorDistributionManager::fs_IsValidNamespaceName(NStr::CStr const &_String)
		{
			return NNet::fg_IsValidHostname(_String, "/");
		}
	}
}
