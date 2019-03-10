// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include "Malterlib_Concurrency_Unwrap.h"

namespace NMib::NConcurrency
{
	NUnwrap::CUnwrapHelper g_Unwrap;
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

	CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer const &_Helper, NStr::CStr const &_Message)
	{
		return CUnwrapHelperWithTransformer
			(
			 	[Message = _Message, fPreviousTransformer = _Helper.m_fTransformer](NException::CExceptionPointer &&_pException)
				{
					auto pException = fPreviousTransformer(fg_Move(_pException));
					try
					{
						std::rethrow_exception(pException);
					}
					catch (NException::CExceptionBase const &_Exception)
					{
						return NException::fg_ExceptionPointer(DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Message, _Exception), fg_Move(pException)));
					}
					return fg_Move(_pException);
				}
			)
		;
	}
}
