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
			CSubSystem_Concurrency_DistributedActor::CSubSystem_Concurrency_DistributedActor()
			{
				fg_ConcurrencyManager(); // Add dependency to subsystem
				
				m_DistributionManager = fg_ConstructActor<CActorDistributionManager>();
			}
			
			CSubSystem_Concurrency_DistributedActor::~CSubSystem_Concurrency_DistributedActor()
			{
			}
			
			void CSubSystem_Concurrency_DistributedActor::f_DestroyThreadSpecific()
			{
				m_DistributionManager.f_Clear();
			}			
			
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

		TCActor<CActorDistributionManager> const &fg_GetDistributionManager()
		{
			return NPrivate::fg_DistributedActorSubSystem().m_DistributionManager;
		}


		
		CActorDistributionCryptographySettings::CActorDistributionCryptographySettings()
		{
		}
		
		CActorDistributionCryptographySettings::~CActorDistributionCryptographySettings()
		{
		}		
		
		void CActorDistributionCryptographySettings::f_GenerateNewCert(NContainer::TCVector<NStr::CStr> const &_HostNames, int32 _KeyBits)
		{
			m_Subject = fg_Format("Malterlib Distributed Actors - {} - {}", NProcess::NPlatform::fg_Process_GetComputerName(), NCryptography::fg_RandomID());
			NNet::CSSLContext::fs_GenerateSelfSignedCertAndKey
				(
					m_Subject
					, _HostNames
					, m_PublicCertificate
					, m_PrivateCertificate
					, _KeyBits
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
			NContainer::TCVector<uint8> Return;
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> KeyData = m_PrivateCertificate;
			NNet::CSSLContext::fs_GenerateClientCertificateRequest
				(
					m_Subject
					, Return
					, KeyData
					, 0
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
					, m_PrivateCertificate
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
				m_PrivateClientCertificate = _Settings.m_PrivateCertificate;
				m_PublicClientCertificate = pRemote->m_PublicClientCertificate;
				m_PublicServerCertificate = pRemote->m_PublicServerCertificate;
			}
		}
		
		CActorDistributionListenSettings::CActorDistributionListenSettings(uint16 _Port)
		{
			NNet::CNetAddressTCPv4 AnyAddress;
			AnyAddress.m_Port = _Port;
			m_ListenAddresses.f_Insert(AnyAddress);
			NNet::CNetAddressTCPv6 AnyAddressV6;
			AnyAddressV6.m_Port = _Port;
			m_ListenAddresses.f_Insert(AnyAddressV6);
		}

		CActorDistributionListenSettings::~CActorDistributionListenSettings()
		{
		}
		
		CActorDistributionListenSettings::CActorDistributionListenSettings(NContainer::TCVector<NNet::CNetAddress> const &_Addresses)
			: m_ListenAddresses(_Addresses)
		{
		}
		
		void CActorDistributionListenSettings::f_SetCryptography(CActorDistributionCryptographySettings const &_Settings)
		{
			m_PublicCertificate = _Settings.m_PublicCertificate;
			m_PrivateCertificate = _Settings.m_PrivateCertificate;
		}		
	}
}
