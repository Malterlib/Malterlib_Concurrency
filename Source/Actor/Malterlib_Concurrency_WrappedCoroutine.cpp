// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_AsyncResult.h"

namespace NMib::NConcurrency::NPrivate
{
	void TCWrappedCoroutinePromise<void>::return_value(CVoidTag &&_Void)
	{
		this->m_Result.f_SetResult();
	}

	void TCWrappedCoroutinePromise<void>::return_value(CVoidTag const &_Void)
	{
		this->m_Result.f_SetResult();
	}
}
