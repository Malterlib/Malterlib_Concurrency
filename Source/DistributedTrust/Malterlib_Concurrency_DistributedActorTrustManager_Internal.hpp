// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturn>
	void CDistributedActorTrustManager::CInternal::f_RunAfterInit(TCPromise<tf_CReturn> const &_Promise, NFunction::TCFunctionMovable<void ()> &&_fToRun)
	{
		if (m_Initialize == EInitialize_Success)
		{
			_fToRun();
			return;
		}
		else if (m_Initialize == EInitialize_Failure)
		{
			_Promise.f_SetException(DMibErrorInstance(fg_Format("Trust manager failed to initialize: {}", m_InitializeError)));
			return;
		}

		(*m_pInitOnce)() > [_Promise, Function = fg_Move(_fToRun)](TCAsyncResult<NStr::CStr> &&_Result) mutable
			{
				if (_Result)
					Function();
				else
					_Promise.f_SetException(DMibErrorInstance(fg_Format("Trust manager failed to initialize: {}", _Result.f_GetExceptionStr())));
			}
		;
	}

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
