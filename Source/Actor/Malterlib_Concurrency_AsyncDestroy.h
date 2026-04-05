// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency
{
	template <typename t_FDestroy>
	struct [[nodiscard("You need to store the async destroy in an variable")]] TCAsyncDestroy
	{
		template <typename tf_FDestroy>
		TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
			requires (!NTraits::cIsReference<t_FDestroy>)
		;

		template <typename tf_FDestroy>
		TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
			requires (NTraits::cIsReference<t_FDestroy>)
		;

		TCAsyncDestroy(TCAsyncDestroy &&_Other);
		~TCAsyncDestroy();

		void f_Clear();

		struct [[nodiscard("You need to co_await the wrapped result")]] CNoUnwrap
		{
			TCFutureAwaiter<void, false> operator co_await() &&;

			TCAsyncDestroy *m_pWrapped;
		};

		CNoUnwrap f_Wrap() &&;

		TCFutureAwaiter<void, true> operator co_await() &&;

	private:
		t_FDestroy mp_fDestroy;
		CFutureCoroutineContext *mp_pCoroutineContext = nullptr;
	};

	template <typename t_FDestroy, typename t_FDestroyFinal>
	struct [[nodiscard("You need to co_await the async destroy")]] TCAsyncDestroyAwaiter
	{
		template <typename tf_FDestroy>
		TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
			requires (!NTraits::cIsReference<t_FDestroy>)
		;

		template <typename tf_FDestroy>
		TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
			requires (NTraits::cIsReference<t_FDestroy>)
		;

		TCAsyncDestroyAwaiter(TCAsyncDestroyAwaiter &&_Other);

		~TCAsyncDestroyAwaiter()
#if DMibEnableSafeCheck > 0
			noexcept(false)
#endif
		;

		inline_always bool await_ready() const noexcept;

		template <typename tf_CContext>
		inline_always bool await_suspend(TCCoroutineHandle<tf_CContext> const &_Handle) noexcept;
		inline_always TCAsyncDestroy<t_FDestroyFinal> await_resume() noexcept;

	private:
		t_FDestroy mp_fDestroy;
		CFutureCoroutineContext *mp_pCoroutineContext = nullptr;
#if DMibEnableSafeCheck > 0
		bool mp_bAwaited = false;
#endif
	};

	TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction> fg_AsyncDestroy(CActorSubscription &&_pToDestroy);

	using CAsyncDestroy = TCAsyncDestroy<FUnitVoidFutureFunction>;
	using CAsyncDestroyAwaiter = TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction>;

	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(FUnitVoidFutureFunction &&_fDestroy);
}

#include "Malterlib_Concurrency_AsyncDestroy.hpp"
