// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <experimental/coroutine>

namespace NMib::NConcurrency
{
	template <typename t_CCoroutineContext = void>
	using TCCoroutineHandle = std::experimental::coroutine_handle<t_CCoroutineContext>;
	//using CNoopCoroutineHandle = std::experimental::noop_coroutine_handle;
	using CSuspendAlways = std::experimental::suspend_always;
	using CSuspendNever = std::experimental::suspend_never;
}
