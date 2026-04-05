// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
