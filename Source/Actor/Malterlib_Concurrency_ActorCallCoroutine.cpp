// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	FExceptionTransformer CActorWithErrorBase::f_GetTransformer() &&
	{
		return fg_Move(m_fTransformer);
	}
}
