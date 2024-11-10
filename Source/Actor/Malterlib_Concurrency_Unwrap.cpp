// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include "Malterlib_Concurrency_Unwrap.h"

namespace NMib::NConcurrency
{
	NUnwrap::CUnwrapHelper g_Unwrap;
	NUnwrap::CUnwrapFirstHelper g_UnwrapFirst;

	TCAsyncResult<void> fg_Unwrap(TCAsyncResult<void> &&_ToUnwrap)
	{
		return fg_Move(_ToUnwrap);
	}

	TCAsyncResult<void> fg_UnwrapFirst(TCAsyncResult<void> &&_ToUnwrap)
	{
		return fg_Move(_ToUnwrap);
	}
}

namespace NMib::NConcurrency::NUnwrap
{
	CUnwrapHelperWithTransformer CUnwrapHelperWithTransformer::ms_EmptyTransformer
		(
			[](NException::CExceptionPointer &&_pException)
			{
				return fg_Move(_pException);
			}
		)
	;

	CUnwrapHelperWithTransformer::CUnwrapHelperWithTransformer(FExceptionTransformer &&_fTransformer)
		: m_fTransformer(fg_Move(_fTransformer))
	{
	}
}
