// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

#include "Malterlib_Concurrency_ExceptionTransform.h"

namespace NMib::NConcurrency
{
	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, NStr::CStr &&_Message
		)
	{
		return [Message = fg_Move(_Message), fPreviousTransformer = fg_Move(_fPreviousTransformer)](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
			{
				auto pException = fPreviousTransformer(fg_Move(_pException));
				if (!pException)
					return nullptr;

				if (NException::fg_ExceptionIsOfType<NException::CExceptionBase>(_pException))
					return NException::fg_MakeException(DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Message, NException::fg_ExceptionString(_pException)), fg_Move(pException), false));
				return fg_Move(_pException);
			}
		;
	}
}
