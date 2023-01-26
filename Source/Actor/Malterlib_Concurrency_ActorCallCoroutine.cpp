// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	void fg_ThrowExceptionCallingAsyncResult(NException::CExceptionPointer &&_Exception)
	{
		DMibErrorCoroutineWrapper("Exception calling async result", fg_Move(_Exception));
	}

	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> CActorWithErrorBase::fp_GetTransformer()
	{
		return [Error = fg_Move(m_Error)](NException::CExceptionPointer &&_pException)
			{
				try
				{
					std::rethrow_exception(_pException);
				}
				catch (NException::CException const &_Exception)
				{
					return NException::fg_ExceptionPointer(DMibErrorInstanceWrapped(NStr::fg_Format("{}: {}", Error, _Exception), fg_Move(_pException), false));
				}
				return fg_Move(_pException);
			}
		;
	}
}
