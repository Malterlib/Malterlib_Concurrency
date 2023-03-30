// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> CActorWithErrorBase::fp_GetTransformer()
	{
		return [Error = fg_Move(m_Error)](NException::CExceptionPointer &&_pException)
			{
				if (NException::fg_ExceptionIsOfType<NException::CExceptionBase>(_pException))
					return NException::fg_MakeException(DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Error, NException::fg_ExceptionString(_pException)), fg_Move(_pException), false));
				return fg_Move(_pException);
			}
		;
	}
}
