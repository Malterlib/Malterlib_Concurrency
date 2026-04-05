// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CReturnValue>
	auto operator % (TCPromise<tf_CReturnValue> const &_Promise, CDistributedAppAuditor const &_Auditor)
	{
		return TCPromiseWithExceptionTransfomer<tf_CReturnValue>(_Promise, fg_ExceptionTransformer(nullptr, _Auditor));
	}

	template <typename tf_CReturnValue>
	auto operator % (TCPromiseWithExceptionTransfomer<tf_CReturnValue> const &_Promise, CDistributedAppAuditor const &_Auditor)
	{
		return TCPromiseWithExceptionTransfomer<tf_CReturnValue>(_Promise.m_Promise, fg_ExceptionTransformer(fg_Move(_Promise.m_fTransformer), _Auditor));
	}
}

#include "Malterlib_Concurrency_DistributedApp_Auditor_Coroutine.hpp"
