// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ExceptionTransform.h"

namespace NMib::NConcurrency
{
	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, NStr::CStr &&_Message
		)
	{
		if (!_fPreviousTransformer)
		{
			return [Message = fg_Move(_Message)](NException::CExceptionPointer &&_pException)
				{
					if (NException::fg_ExceptionIsOfType<NException::CExceptionBase>(_pException))
					{
						auto ExceptionString = NException::fg_ExceptionString(_pException);
						return NException::fg_MakeException
							(
								DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Message, ExceptionString), fg_Move(_pException), false)
							)
						;
					}
					return fg_Move(_pException);
				}
			;
		}
		else
		{
			return [Message = fg_Move(_Message), fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
				{
					auto pException = fPreviousTransformer(fg_Move(_pException));
					if (!pException)
						return nullptr;

					if (NException::fg_ExceptionIsOfType<NException::CExceptionBase>(_pException))
					{
						auto ExceptionString = NException::fg_ExceptionString(_pException);
						return NException::fg_MakeException
							(
								DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Message, ExceptionString), fg_Move(pException), false)
							)
						;
					}
					return fg_Move(_pException);
				}
			;
		}
	}

	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, NStr::CStr const &_Message
		)
	{
		return fg_ExceptionTransformer(fg_Move(_fPreviousTransformer), fg_TempCopy(_Message));
	}
}
