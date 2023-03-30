// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Exception/Exception>

namespace NMib::NConcurrency
{
	NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fg_ExceptionTransformer
		(
			NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fPreviousTransformer
			, NStr::CStr &&_Message
		)
	;
}
