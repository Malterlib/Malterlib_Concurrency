// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	CDistributedActorTrustManager_Address const &CDistributedActorTrustManager::CInternal::CConnectionState::f_GetAddress() const
	{
		return NContainer::TCMap<CDistributedActorTrustManager_Address, CConnectionState>::fs_GetKey(*this);
	}

	NStr::CStr const &CDistributedActorTrustManager::CInternal::CHostState::f_GetHostID() const
	{
		return NContainer::TCMap<NStr::CStr, CHostState>::fs_GetKey(*this);
	}

	NStr::CStr const &CDistributedActorTrustManager::CInternal::CNamespaceState::f_GetNamespaceName() const
	{
		return NContainer::TCMap<NStr::CStr, CNamespaceState>::fs_GetKey(*this);
	}

	CPermissionIdentifiers const &CDistributedActorTrustManager::CInternal::CPermissionState::f_GetIdentity() const
	{
		return NContainer::TCMap<CPermissionIdentifiers, CPermissionState>::fs_GetKey(*this);
	}
}
