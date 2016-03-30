// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib
{
	namespace NConcurrency
	{
		void CActorDistributionManager::f_SetSecurity(CDistributedActorSecurity const &_Security)
		{
			auto &Internal = *mp_pInternal;
			
			for (auto const &Namespace : _Security.m_AllowedIncomingConnectionNamespaces)
			{
				Internal.m_AllowedIncomingConnectionNamespaces[Namespace];
				Internal.m_AllowedBothNamespaces[Namespace];
			}
			for (auto const &Namespace : _Security.m_AllowedOutgoingConnectionNamespaces)
			{
				Internal.m_AllowedOutgoingConnectionNamespaces[Namespace];
				Internal.m_AllowedBothNamespaces[Namespace];
			}
		}
		
		NContainer::TCSet<NStr::CStr> const &CActorDistributionManager::CInternal::fp_GetAllowedNamespacesForHost(CHost *_pHost, bool &o_bAllowAll)
		{
			if (_pHost->m_bIncoming && _pHost->m_bOutgoing)
			{
				if (m_AllowedOutgoingConnectionNamespaces.f_IsEmpty())
					o_bAllowAll = true;
				return m_AllowedBothNamespaces;
			}
			else if (_pHost->m_bIncoming)
				return m_AllowedIncomingConnectionNamespaces;
			else if (_pHost->m_bOutgoing)
			{
				if (m_AllowedOutgoingConnectionNamespaces.f_IsEmpty())
					o_bAllowAll = true;
				return m_AllowedOutgoingConnectionNamespaces;
			}
			
			DMibNeverGetHere;
			return m_AllowedOutgoingConnectionNamespaces;
		}		
	}
}
