// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_Manager.h"
#include "Malterlib_Concurrency_Promise.h"
#include <Mib/Log/Log>

namespace NMib::NConcurrency
{
	DMibImpErrorClassImplement(CExceptionCoroutineWrapper);
}

namespace NMib::NConcurrency::NPrivate
{
	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception)
	{
		DMibDTrace("Unobserved exception in future: {}\n", NException::fg_ExceptionString(_Exception));
		DMibLog(Error, "Unobserved exception in future: {}", NException::fg_ExceptionString(_Exception));
	}
}
