// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Promise.h"

namespace NMib::NConcurrency
{
	template <typename t_FDestroy>
	struct [[nodiscard("You need to store the async destroy in an variable")]] TCAsyncDestroy
	{
		template <typename tf_FDestroy>
		TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
			requires (!NTraits::TCIsReference<t_FDestroy>::mc_Value)
		;

		template <typename tf_FDestroy>
		TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
			requires (NTraits::TCIsReference<t_FDestroy>::mc_Value)
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
			requires (!NTraits::TCIsReference<t_FDestroy>::mc_Value)
		;

		template <typename tf_FDestroy>
		TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
			requires (NTraits::TCIsReference<t_FDestroy>::mc_Value)
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

	template <typename tf_FDestroy>
	auto fg_AsyncDestroyByValue(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	;

	template <typename tf_FDestroy>
	auto fg_AsyncDestroy(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	;

	template <typename tf_FDestroy>
	auto fg_AsyncDestroyLogError(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	;

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroy(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy().f_DiscardResult();
		}
	;

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroy(tf_CToCleanup &&_pToDestroy)
		requires requires
		{
			_pToDestroy->f_Destroy().f_DiscardResult();
		}
	;

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroyLogError(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy().f_DiscardResult();
		}
	;

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroyLogError(tf_CToCleanup &&_pToDestroy)
		requires requires
		{
			_pToDestroy->f_Destroy().f_DiscardResult();
		}
	;

	template <typename tf_CActor>
	auto fg_AsyncDestroy(TCActor<tf_CActor> &_ToDestroy);
	
	template <typename tf_CActor>
	auto fg_AsyncDestroyByValue(TCActor<tf_CActor> &_ToDestroy);

	TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction> fg_AsyncDestroy(CActorSubscription &&_pToDestroy);

	using CAsyncDestroy = TCAsyncDestroy<FUnitVoidFutureFunction>;
	using CAsyncDestroyAwaiter = TCAsyncDestroyAwaiter<FUnitVoidFutureFunction, FUnitVoidFutureFunction>;

	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(FUnitVoidFutureFunction &&_fDestroy);

	template <typename tf_CToCleanup>
	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(tf_CToCleanup &_ToDestroy);
}

#include "Malterlib_Concurrency_AsyncDestroy.hpp"
