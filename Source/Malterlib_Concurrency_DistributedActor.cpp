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
				CSubSystem_Concurrency_DistributedActorDefaultManager(NStr::CStr const &_HostID);
				~CSubSystem_Concurrency_DistributedActorDefaultManager();
				void f_DestroyThreadSpecific() override;

				NStr::CStr m_DistributionManagerHostID;
				TCActor<CActorDistributionManager> m_DistributionManager;
			};
			
			CSubSystem_Concurrency_DistributedActorDefaultManager::CSubSystem_Concurrency_DistributedActorDefaultManager()
			{
				DMibError("You need to call fg_InitDistributionManager before using distributed actors");
			}
			
 			CSubSystem_Concurrency_DistributedActorDefaultManager::CSubSystem_Concurrency_DistributedActorDefaultManager(NStr::CStr const &_HostID)
				: m_DistributionManagerHostID(_HostID)
			{
				fg_ConcurrencyManager(); // Add dependency to subsystem
				fg_RuntimeTypeRegistry();
				
				m_DistributionManager = fg_ConstructActor<CActorDistributionManager>(_HostID);
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
						"1.3.6.1.4.1.555555.1.1"
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
			void fg_CopyReplyToContinuation(TCContinuation<void> &_Continuation, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data)
			{
				NStream::CBinaryStreamMemoryPtr<> ReplyStream;
				ReplyStream.f_OpenRead(_Data);
				if (fg_CopyReplyToContinuationShared(ReplyStream, _Continuation))
					return;
				
				_Continuation.f_SetResult();
			}
		}
		
		void fg_InitDistributedActorSystem()
		{
			NPrivate::fg_DistributedActorSubSystem();
		}

		CCallingHostInfo const &CActorDistributionManager::fs_GetCallingHostInfo()
		{
			return NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal->m_CallingHostInfo;
		}
		
		CCallingHostInfo const &fg_GetCallingHostInfo()
		{
			return CActorDistributionManager::fs_GetCallingHostInfo();
		}

		NStr::CStr fg_InitDistributionManager(NStr::CStr const &_HostID)
		{
			if (NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager.f_WasCreated())
				return NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager->m_DistributionManagerHostID;
			NPrivate::g_MalterlibSubSystem_Concurrency_DistributedActorDefaultManager.f_Construct
				(
					[&](void *_pMemory) -> NPrivate::CSubSystem_Concurrency_DistributedActorDefaultManager *
					{
						return new(_pMemory) NPrivate::CSubSystem_Concurrency_DistributedActorDefaultManager(_HostID);
					}
				)
			;
			
			return _HostID;
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
		
		void CActorDistributionCryptographySettings::f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, int32 _KeyBits)
		{
			NPrivate::fg_DistributedActorSubSystem(); // Register extension if needed

			m_Subject = fg_Format("Malterlib Distributed Actors - {}", NCryptography::fg_RandomID());
			
			NNet::CSSLContext::CCertificateOptions Options;
			Options.m_KeyLength = _KeyBits;
			Options.m_Hostnames = _HostNames;
			Options.m_Subject = m_Subject; 
			auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
			Extension.m_bCritical = false; 
			Extension.m_Value = m_HostID;
			
			NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey
				(
					Options
					, m_PublicCertificate
					, m_PrivateKey
					, 1
					, 10*365
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
			NContainer::TCVector<uint8> Return;
			NNet::CSSLContext::fs_SignClientCertificate
				(
					m_PublicCertificate
					, m_PrivateKey
					, _Request
					, Return
					, ++m_Serial
					, 10*365
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
				, NStr::CStr const &_RealHostID
				, NStr::CStr const &_LastExecutionID
			)
			: mp_DistributionManager(_DistributionManager)
			, mp_UniqueHostID(_UniqueHostID) 
			, mp_RealHostID(_RealHostID)
			, mp_LastExecutionID(_LastExecutionID)
		{
		}
		
		NStr::CStr const &CCallingHostInfo::f_GetRealHostID() const
		{
			return mp_RealHostID;
		}
		
		NStr::CStr const &CCallingHostInfo::f_GetUniqueHostID() const
		{
			return mp_UniqueHostID;
		}

		bool CCallingHostInfo::operator ==(CCallingHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(mp_UniqueHostID, mp_RealHostID, mp_LastExecutionID) 
				== NContainer::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_RealHostID, _Right.mp_LastExecutionID)
			; 
		}		

		bool CCallingHostInfo::operator <(CCallingHostInfo const &_Right) const
		{
			return NContainer::fg_TupleReferences(mp_UniqueHostID, mp_RealHostID, mp_LastExecutionID) 
				< NContainer::fg_TupleReferences(_Right.mp_UniqueHostID, _Right.mp_RealHostID, _Right.mp_LastExecutionID)
			; 
		}		
		
		TCActor<CActorDistributionManager> const &CCallingHostInfo::f_GetDistributionManager() const
		{
			return mp_DistributionManager;
		}
		
		TCDispatchedActorCall<CActorSubscription> CCallingHostInfo::f_OnDisconnect(TCActor<CActor> const &_Actor, NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fOnDisconnect) const
		{
			return fg_ConcurrentDispatch
				(
					[DistributionManager = mp_DistributionManager, fOnDisconnect = fg_Move(_fOnDisconnect), UniqueHostID = mp_UniqueHostID, LastExecutionID = mp_LastExecutionID, _Actor]
					{
						TCContinuation<CActorSubscription> Continuation;
						DistributionManager(&CActorDistributionManager::fp_OnRemoteDisconnect, _Actor, fg_Move(fOnDisconnect), UniqueHostID, LastExecutionID) > Continuation;
						return Continuation;
					}
				)
			;
		}
	}
}
