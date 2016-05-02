// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedActorTrustManager::CInternal
		{
		};
		
		NStr::CStr CDistributedActorTrustManager::CTrustTicket::f_ToStringTicket() const
		{
			return "";
		}
		CDistributedActorTrustManager::CTrustTicket CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(NStr::CStr const &_StringTicket)
		{
			return CTrustTicket();
		}

		CDistributedActorTrustManager::CDistributedActorTrustManager
			(
				NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_DatabaseAccessor
				, NFunction::TCFunction
				<
					NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID)
				> const &_fConstructManager
			)
		{
		}
			
		CDistributedActorTrustManager::~CDistributedActorTrustManager()
		{
		}
			
		TCContinuation<void> CDistributedActorTrustManager::f_AddListen(CDistributedActorTrustManager_Address const &_Address)
		{
			return fg_Default();
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_RemoveListen(CDistributedActorTrustManager_Address const &_Address)
		{
			return fg_Default();
		}

		TCContinuation<CDistributedActorTrustManager::CTrustTicket> CDistributedActorTrustManager::f_GenerateConnectionTicket(CDistributedActorTrustManager_Address const &_Address)
		{
			return fg_Default();
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_RemoveClient(NStr::CStr const &_HostID)
		{
			return fg_Default();
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_AddClientConnection(CTrustTicket const &_TrustTicket)
		{
			return fg_Default();
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
		{
			return fg_Default();
		}
		
		bool CDistributedActorTrustManager_Address::operator == (CDistributedActorTrustManager_Address const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_URL, m_PreferType) == NContainer::fg_TupleReferences(_Right.m_URL, _Right.m_PreferType); 
		}

		bool CDistributedActorTrustManager_Address::operator < (CDistributedActorTrustManager_Address const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_URL, m_PreferType) < NContainer::fg_TupleReferences(_Right.m_URL, _Right.m_PreferType); 
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
