// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Network/SSL>

namespace NMib
{
	namespace NConcurrency
 	{
		CDistributedActorTrustManager::CDistributedActorTrustManager
			(
				NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
				, NFunction::TCFunction
				<
					NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
				> const &_fConstructManager
				, uint32 _KeySize
				, NNet::ENetFlag _ListenFlags
				, NStr::CStr const &_FriendlyName
			)
			: mp_pInternal(fg_Construct(this, _Database, _fConstructManager, _KeySize, _ListenFlags, _FriendlyName))
		{
		}
			
		CDistributedActorTrustManager::~CDistributedActorTrustManager()
		{
		}
		
		CDistributedActorTrustManager::CInternal::CInternal
			(
				CDistributedActorTrustManager *_pThis
				, NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
				, NFunction::TCFunction
				<
					NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName)
				> const &_fConstructManager
				, uint32 _KeySize
				, NNet::ENetFlag _ListenFlags
				, NStr::CStr const &_FriendlyName
			)
			: m_pThis(_pThis)
			, m_Database(_Database)
			, m_fDistributionManagerFactory(_fConstructManager)
			, m_KeySize(_KeySize)
			, m_ListenFlags(_ListenFlags)
			, m_FriendlyName(_FriendlyName)
		{
		}
		
		CDistributedActorTrustManager::CInternal::~CInternal()
		{
			*m_pDestroyed = true;
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_Destroy()
		{
			auto &Internal = *mp_pInternal;
			TCActorResultVector<void> Stops;
			for (auto &Connection : Internal.m_ClientConnections)
				Connection.m_ConnectionReference.f_Disconnect() > Stops.f_AddResult();
			
			for (auto &Listen : Internal.m_Listen)
				Listen.m_ListenReference.f_Stop() > Stops.f_AddResult();
			
			TCContinuation<void> Continuation;
			
			Stops.f_GetResults() > Continuation / [Continuation](NContainer::TCVector<TCAsyncResult<void>> &&_StopResults)
				{
					for (auto &Result : _StopResults)
					{
						if (!Result)
						{
							DMibLogWithCategory
								(
									Mib/Concurrency/Trust
									, Error
									, "Failed to stop connection or listen: {}"
									, Result.f_GetExceptionStr()
								)
							;
						}
					}
					Continuation.f_SetResult();
				}
			;
			
			return Continuation;
		}
		
		NStr::CStr CDistributedActorTrustManager::CTrustTicket::f_ToStringTicket() const
		{
			auto Data = NStream::fg_ToByteVector(*this);
			return NDataProcessing::fg_Base64Encode(Data);
		}
		
		CDistributedActorTrustManager::CTrustTicket CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(NStr::CStr const &_StringTicket)
		{
			NContainer::TCVector<uint8> Data;
			NDataProcessing::fg_Base64Decode(_StringTicket, Data);
			return NStream::fg_FromByteVector<CTrustTicket>(Data);
		}

		bool CDistributedActorTrustManager_Address::operator == (CDistributedActorTrustManager_Address const &_Right) const
		{
			return m_URL == _Right.m_URL; 
		}

		bool CDistributedActorTrustManager_Address::operator < (CDistributedActorTrustManager_Address const &_Right) const
		{
			return m_URL < _Right.m_URL; 
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_RemoveClient(NStr::CStr const &_HostID)
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<void> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation, _HostID]
					{
						auto &Internal = *mp_pInternal;
						Internal.m_Database
							(
								&ICDistributedActorTrustManagerDatabase::f_RemoveClient
								, _HostID
							)
							> Continuation % "Failed to remove client from trust database" / [this, Continuation, _HostID]()
							{
								auto &Internal = *mp_pInternal;
								Internal.m_ActorDistributionManager(&CActorDistributionManager::f_KickHost, _HostID) > Continuation % "Failed to kick host from distribution manager";
							}
						;
					}
				)
			;
			return Continuation;
		}

		auto CDistributedActorTrustManager::f_EnumClients() -> TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>>
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<NContainer::TCMap<NStr::CStr, CHostInfo>> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation]
					{
						auto &Internal = *mp_pInternal;
						Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_EnumClients, true) 
							> (Continuation % "Failed to enum clients in database") / [Continuation](NContainer::TCMap<NStr::CStr, CClient> &&_Info)
							{
								NContainer::TCMap<NStr::CStr, CHostInfo> Clients;
								for (auto iClient = _Info.f_GetIterator(); iClient; ++iClient)
								{
									auto &Client = Clients[iClient.f_GetKey()];
									Client.m_HostID = iClient.f_GetKey();
									Client.m_FriendlyName = iClient->m_LastFriendlyName;
								}
								Continuation.f_SetResult(fg_Move(Clients));
							}
						;
					}
				)
			;
			return Continuation;
		}
		
		TCContinuation<bool> CDistributedActorTrustManager::f_HasClient(NStr::CStr const &_HostID)
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<bool> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation, _HostID]
					{
						auto &Internal = *mp_pInternal;
						Internal.m_Database
							(
								&ICDistributedActorTrustManagerDatabase::f_HasClient
								, _HostID
							)
							> Continuation % "Failed to check for client existence" / [this, Continuation, _HostID](bool _bExists)
							{
								Continuation.f_SetResult(_bExists);
							}
						;
					}
				)
			;
			return Continuation;
		}
		
		TCContinuation<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> CDistributedActorTrustManager::f_GetDistributionManager() const
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation]
					{
						auto &Internal = *mp_pInternal;
						Continuation.f_SetResult(Internal.m_ActorDistributionManager);
					}
				)
			;
			return Continuation;
		}
		
		TCContinuation<NStr::CStr> CDistributedActorTrustManager::f_GetHostID() const
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<NStr::CStr> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation]
					{
						auto &Internal = *mp_pInternal;
						Continuation.f_SetResult(Internal.m_BasicConfig.m_HostID);
					}
				)
			;
			return Continuation;
		}
		
		CHostInfo NDistributedActorTrustManagerDatabase::CClientConnection::f_GetHostInfo() const
		{
			CHostInfo Info;
			try
			{
				Info.m_HostID = CActorDistributionManager::fs_GetCertificateHostID(m_PublicServerCertificate);
			}
			catch (NException::CException const &)
			{
			}
			Info.m_FriendlyName = m_LastFriendlyName;
			return Info;
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
