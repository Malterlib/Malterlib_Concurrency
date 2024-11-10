// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Exception/Exception>

namespace NMib::NConcurrency
{
	using FExceptionTransformer = NFunction::TCFunction
		<
			NException::CExceptionPointer (NException::CExceptionPointer &&_pException)
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, sizeof(void *)
				, sizeof(void *)
				, false
			>
		>
	;

	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, NStr::CStr &&_Message
		)
	;

	FExceptionTransformer fg_ExceptionTransformer
		(
			FExceptionTransformer &&_fPreviousTransformer
			, NStr::CStr const &_Message
		)
	;
}
