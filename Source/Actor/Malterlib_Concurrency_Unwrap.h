// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ExceptionTransform.h"

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
		CUnwrapHelperWithTransformer(FExceptionTransformer &&_fTransformer);

		FExceptionTransformer m_fTransformer;

		static CUnwrapHelperWithTransformer ms_EmptyTransformer;
	};
}

namespace NMib::NConcurrency
{
	extern NUnwrap::CUnwrapHelper g_Unwrap;
	extern NUnwrap::CUnwrapFirstHelper g_UnwrapFirst;
}

#include "Malterlib_Concurrency_Unwrap.hpp"
