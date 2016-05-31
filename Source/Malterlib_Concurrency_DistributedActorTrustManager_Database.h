// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyDefines>
#include "Malterlib_Concurrency_Actor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"

namespace NMib
{
	namespace NConcurrency
	{
		namespace NDistributedActorTrustManagerDatabase
		{
			struct CBasicConfig
			{
				enum 
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);

				NStr::CStr m_HostID;

				NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_CAPrivateKey;
				NContainer::TCVector<uint8> m_CACertificate;
			};

			struct CServerCertificate
			{
				enum 
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);

				NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> m_PrivateKey;
				NContainer::TCVector<uint8> m_PublicCertificate;
			};
			
			struct CListenConfig
			{
				enum 
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);
				inline_always bool operator == (CListenConfig const &_Right) const;
				inline_always bool operator < (CListenConfig const &_Right) const;

				CDistributedActorTrustManager_Address m_Address;
			};

			struct CClient
			{
				enum 
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);

				NContainer::TCVector<uint8> m_PublicCertificate;
			};

			struct CClientConnection
			{
				enum 
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);

				NContainer::TCVector<uint8> m_PublicServerCertificate;
				NContainer::TCVector<uint8> m_PublicClientCertificate;
			};
		}
		
		class ICDistributedActorTrustManagerDatabase : public NConcurrency::CActor
		{
		public:
			using CBasicConfig = NDistributedActorTrustManagerDatabase::CBasicConfig;
			using CServerCertificate = NDistributedActorTrustManagerDatabase::CServerCertificate;
			using CListenConfig = NDistributedActorTrustManagerDatabase::CListenConfig;
			using CClient = NDistributedActorTrustManagerDatabase::CClient;
			using CClientConnection = NDistributedActorTrustManagerDatabase::CClientConnection;
			
			virtual TCContinuation<CBasicConfig> f_GetBasicConfig() pure;
			virtual TCContinuation<void> f_SetBasicConfig(CBasicConfig const &_BasicConfig) pure;
			virtual TCContinuation<int32> f_GetNewCertificateSerial() pure;
			
			virtual TCContinuation<NContainer::TCSet<NStr::CStr>> f_EnumServerCertificates() pure;
			virtual TCContinuation<CServerCertificate> f_GetServerCertificate(NStr::CStr const &_HostName) pure;
			virtual TCContinuation<void> f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) pure;
			virtual TCContinuation<void> f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate) pure;
			virtual TCContinuation<void> f_RemoveServerCertificate(NStr::CStr const &_HostName) pure;
			
			virtual TCContinuation<NContainer::TCSet<CListenConfig>> f_EnumListenConfigs() pure;
			virtual TCContinuation<void> f_AddListenConfig(CListenConfig const &_Config) pure;
			virtual TCContinuation<void> f_RemoveListenConfig(CListenConfig const &_Config) pure;

			virtual TCContinuation<NContainer::TCSet<NStr::CStr>> f_EnumClients() pure;
 			virtual TCContinuation<CClient> f_GetClient(NStr::CStr const &_HostID) pure;
 			virtual TCContinuation<NPtr::TCUniquePointer<CClient>> f_TryGetClient(NStr::CStr const &_HostID) pure;
 			virtual TCContinuation<bool> f_HasClient(NStr::CStr const &_HostID) pure;
			virtual TCContinuation<void> f_AddClient(NStr::CStr const &_HostID, CClient const &_Client) pure;
			virtual TCContinuation<void> f_SetClient(NStr::CStr const &_HostID, CClient const &_Client) pure;
			virtual TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID) pure;
			
			virtual TCContinuation<NContainer::TCSet<CDistributedActorTrustManager_Address>> f_EnumClientConnections() pure;
			virtual TCContinuation<CClientConnection> f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address) pure;
			virtual TCContinuation<void> f_AddClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) pure;
			virtual TCContinuation<void> f_SetClientConnection(CDistributedActorTrustManager_Address const &_Address, CClientConnection const &_ClientConnection) pure;
			virtual TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address) pure;
		};
	}
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.hpp"
