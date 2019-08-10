// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency
{
	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass *_pClassPtr, tf_CReturn (tf_CClass::*_pPtr) (tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn;

	template <typename tf_CClass, typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CClass &_ClassRef, tf_CReturn (tf_CClass::*_pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn;

	template <typename tf_CReturn, typename ...tfp_CFuncPtrParams, typename ...tfp_CParams>
	mark_artificial inline_always auto fg_CallSafe(tf_CReturn (* _pPtr)(tfp_CFuncPtrParams ...), tfp_CParams &&...p_Params) -> tf_CReturn;
}

#include "Malterlib_Concurrency_CallSafe.hpp"
