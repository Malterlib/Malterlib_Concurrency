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
					NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID)
				> const &_fConstructManager
				,  uint32 _KeySize
			)
			: mp_pInternal(fg_Construct(this, _Database, _fConstructManager, _KeySize))
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
					NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID)
				> const &_fConstructManager
				, uint32 _KeySize
			)
			: m_pThis(_pThis)
			, m_Database(_Database)
			, m_fDistributionManagerFactory(_fConstructManager)
			, m_KeySize(_KeySize)
		{
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
			return NContainer::fg_TupleReferences(m_URL, m_PreferType) == NContainer::fg_TupleReferences(_Right.m_URL, _Right.m_PreferType); 
		}

		bool CDistributedActorTrustManager_Address::operator < (CDistributedActorTrustManager_Address const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_URL, m_PreferType) < NContainer::fg_TupleReferences(_Right.m_URL, _Right.m_PreferType); 
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
							> [this, Continuation, _HostID](TCAsyncResult<void> &&_Result)
							{
								if (!_Result)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to remove client from trust database: {}", _Result.f_GetExceptionStr())));
									return;
								}
								auto &Internal = *mp_pInternal;
								Internal.m_ActorDistributionManager(&CActorDistributionManager::f_KickHost, _HostID) > [Continuation](TCAsyncResult<void> &&_Result) 
									{
										if (!_Result)
										{
											Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to kick host from distribution manager: {}", _Result.f_GetExceptionStr())));
											return;
										}
										Continuation.f_SetResult();
									}
								;
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
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
