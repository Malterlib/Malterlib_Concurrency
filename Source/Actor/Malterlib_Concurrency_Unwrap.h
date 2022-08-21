// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NUnwrap
{
	struct CUnwrapHelperWithTransformer;

	struct CUnwrapHelper
	{
		operator CUnwrapHelperWithTransformer const &();
	};

	struct CUnwrapFirstHelper
	{
	};

	struct CUnwrapHelperWithTransformer
	{
		CUnwrapHelperWithTransformer(NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> &&_fTransformer);

		NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> m_fTransformer;

		static CUnwrapHelperWithTransformer ms_EmptyTransformer;
	};
}

namespace NMib::NConcurrency
{
	extern NUnwrap::CUnwrapHelper g_Unwrap;
	extern NUnwrap::CUnwrapFirstHelper g_UnwrapFirst;
}

#include "Malterlib_Concurrency_Unwrap.hpp"
