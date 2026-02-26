// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NActorDistributionManagerInternal;

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

	NContainer::TCSet<NStr::CStr> const &CActorDistributionManagerInternal::fp_GetAllowedNamespacesForHost(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, bool &o_bAllowAll)
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

	void CActorDistributionManager::f_SetAccessHandler(TCActor<ICActorDistributionManagerAccessHandler> const &_AccessHandler)
	{
		auto &Internal = *mp_pInternal;
		Internal.m_AccessHandler = _AccessHandler;
	}

	void CActorDistributionManager::f_SetHostnameTranslate(NContainer::TCMap<NStr::CStr, NStr::CStr> const &_TranslateHostnames)
	{
		auto &Internal = *mp_pInternal;
		Internal.m_TranslateHostnames = _TranslateHostnames;
	}

	TCFuture<void> CActorDistributionManager::f_SetAuthenticationHandler
		(
			NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> _pHost
			, NStr::CStr _LastExecutionID
			, TCDistributedActor<ICDistributedActorAuthenticationHandler> _AuthenticationHandler
			, NStr::CStr _UserID
			, NStr::CStr _UserName
			, uint32 _HandlerID
		)
	{
		auto &Host = *(static_cast<CHost *>(_pHost.f_Get()));
		if (Host.m_bDeleted.f_Load(NAtomic::gc_MemoryOrder_Relaxed) || Host.m_LastExecutionID != _LastExecutionID)
			co_return {};

		auto &Entry = Host.m_AuthenticationHandlers[_HandlerID];
		Entry.m_Handler = _AuthenticationHandler;
		Entry.m_ClaimedUserID = _UserID;
		Entry.m_ClaimedUserName = _UserName;

		co_return {};
	}

	TCFuture<void> CActorDistributionManager::f_RemoveAuthenticationHandler
		(
			NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> _pHost
			, NStr::CStr _LastExecutionID
			, uint32 _HandlerID
		)
	{
		auto &Host = *(static_cast<CHost *>(_pHost.f_Get()));
		if (Host.m_bDeleted.f_Load(NAtomic::gc_MemoryOrder_Relaxed) || Host.m_LastExecutionID != _LastExecutionID)
			co_return {};

		Host.m_AuthenticationHandlers.f_Remove(_HandlerID);

		co_return {};
	}
}
