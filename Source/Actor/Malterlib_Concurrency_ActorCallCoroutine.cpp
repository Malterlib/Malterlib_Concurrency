// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	CActorWithErrorBase::CActorWithErrorBase(FExceptionTransformer &&_fTransfomer)
		: m_fTransformer(fg_Move(_fTransfomer))
	{
	}

	FExceptionTransformer CActorWithErrorBase::f_GetTransformer() &&
	{
		return fg_Move(m_fTransformer);
	}
}
