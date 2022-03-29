// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <coroutine>

namespace NMib::NConcurrency
{
	template <typename t_CCoroutineContext = void>
	using TCCoroutineHandle = std::coroutine_handle<t_CCoroutineContext>;
	//using CNoopCoroutineHandle = std::noop_coroutine_handle;
	using CSuspendAlways = std::suspend_always;
	using CSuspendNever = std::suspend_never;
}
