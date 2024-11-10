// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	FExceptionTransformer CActorWithErrorBase::f_GetTransformer() &&
	{
		return fg_Move(m_fTransformer);
	}
}
