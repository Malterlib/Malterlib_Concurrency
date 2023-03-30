// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include "Malterlib_Concurrency_Unwrap.h"

namespace NMib::NConcurrency
{
	NUnwrap::CUnwrapHelper g_Unwrap;
	NUnwrap::CUnwrapFirstHelper g_UnwrapFirst;

	void fg_Unwrap(TCAsyncResult<void> &&_ToUnwrap)
	{
		if (_ToUnwrap)
			return;

		NException::CDisableExceptionTraceScope DisableExceptionTrace;
		DMibErrorCoroutineWrapper("Exception unwrapping async result", _ToUnwrap.f_GetException());
	}

	void fg_UnwrapFirst(TCAsyncResult<void> &&_ToUnwrap)
	{
		return fg_Unwrap(fg_Move(_ToUnwrap));
	}

	void fg_Unwrap
		(
			TCAsyncResult<void> &&_ToUnwrap
			, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		if (_ToUnwrap)
			return;

		NException::CDisableExceptionTraceScope DisableExceptionTrace;
		DMibErrorCoroutineWrapper("Exception unwrapping async result", _fTransformer(_ToUnwrap.f_GetException()));
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

	CUnwrapHelperWithTransformer::CUnwrapHelperWithTransformer(NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fTransformer)
		: m_fTransformer(fg_Move(_fTransformer))
	{
	}
}
