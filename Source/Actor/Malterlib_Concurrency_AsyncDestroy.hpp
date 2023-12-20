// Copyright © 2023 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_FDestroy>
	template <typename tf_FDestroy>
	TCAsyncDestroy<t_FDestroy>::TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
		requires (!NTraits::TCIsReference<t_FDestroy>::mc_Value)
		: mp_fDestroy(fg_Forward<tf_FDestroy>(_fDestroy))
		, mp_pCoroutineContext(_pCoroutineContext)
	{
	}

	template <typename t_FDestroy>
	template <typename tf_FDestroy>
	TCAsyncDestroy<t_FDestroy>::TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
		requires (NTraits::TCIsReference<t_FDestroy>::mc_Value)
		: mp_fDestroy(_fDestroy)
		, mp_pCoroutineContext(_pCoroutineContext)
	{
	}

	template <typename t_FDestroy>
	TCAsyncDestroy<t_FDestroy>::TCAsyncDestroy(TCAsyncDestroy &&_Other)
		: mp_fDestroy(fg_Move(_Other.mp_fDestroy))
		, mp_pCoroutineContext(fg_Exchange(_Other.mp_pCoroutineContext, nullptr))
	{
	}

	template <typename t_FDestroy>
	TCAsyncDestroy<t_FDestroy>::~TCAsyncDestroy()
	{
		auto pCoroContext = fg_Exchange(mp_pCoroutineContext, nullptr);
		if (!pCoroContext)
			return;

		if constexpr (NTraits::TCIsReference<t_FDestroy>::mc_Value)
			pCoroContext->m_AsyncDestructors.f_Insert(fg_Move(mp_fDestroy).f_Destroy());
		else
			pCoroContext->m_AsyncDestructors.f_Insert(fg_CallSafe(fg_Move(mp_fDestroy)));
	}

	template <typename t_FDestroy>
	void TCAsyncDestroy<t_FDestroy>::f_Clear()
	{
		mp_pCoroutineContext = nullptr;
	}

	template <typename t_FDestroy>
	TCFutureAwaiter<void, false> TCAsyncDestroy<t_FDestroy>::CNoUnwrap::operator co_await() &&
	{
		DMibFastCheck(m_pWrapped->mp_pCoroutineContext);
		m_pWrapped->mp_pCoroutineContext = nullptr;
		if constexpr (NTraits::TCIsReference<t_FDestroy>::mc_Value)
			return fg_Move(m_pWrapped->mp_fDestroy).f_Destroy();
		else
			return {fg_CallSafe(fg_Move(m_pWrapped->mp_fDestroy)), nullptr};
	}

	template <typename t_FDestroy>
	auto TCAsyncDestroy<t_FDestroy>::f_Wrap() && -> CNoUnwrap
	{
		return {.m_pWrapped = this};
	}

	template <typename t_FDestroy>
	TCFutureAwaiter<void, true> TCAsyncDestroy<t_FDestroy>::operator co_await() &&
	{
		DMibFastCheck(mp_pCoroutineContext);
		mp_pCoroutineContext = nullptr;
		if constexpr (NTraits::TCIsReference<t_FDestroy>::mc_Value)
			return fg_Move(mp_fDestroy).f_Destroy();
		else
			return {fg_CallSafe(fg_Move(mp_fDestroy)), nullptr};
	}


	template <typename t_FDestroy, typename t_FDestroyFinal>
	template <typename tf_FDestroy>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
		requires (!NTraits::TCIsReference<t_FDestroy>::mc_Value)
		: mp_fDestroy(fg_Forward<tf_FDestroy>(_fDestroy))
	{
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	template <typename tf_FDestroy>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
		requires (NTraits::TCIsReference<t_FDestroy>::mc_Value)
		: mp_fDestroy(_fDestroy)
	{
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::TCAsyncDestroyAwaiter(TCAsyncDestroyAwaiter &&_Other)
		: mp_fDestroy(fg_Move(_Other.mp_fDestroy))
	{
		DMibFastCheck(!_Other.mp_pCoroutineContext);
		DMibFastCheck(!fg_Exchange(_Other.mp_bAwaited, true));
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::~TCAsyncDestroyAwaiter()
#if DMibEnableSafeCheck > 0
		noexcept(false)
#endif
	{
		DMibSafeCheck(fg_Exchange(mp_bAwaited, true), "You forgot to co_await the async destroy");
		// Correct usage:
		/*
			CClass Object;

			auto AsyncDestroy = co_await fg_AsyncDestroy(Object);
			// Or
			auto AsyncDestroy = co_await fg_AsyncDestroy
				(
					[&]() -> TCFuture<void>
					{
						// Keep object alive in this frame
						auto ObjectMove = fg_Move(Object);

						co_await ObjectMove.f_Destroy();

						co_return {};
					}
				)
			;
		 */
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	inline_always bool TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::await_ready() const noexcept
	{
		return false;
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	template <typename tf_CContext>
	inline_always bool TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::await_suspend(TCCoroutineHandle<tf_CContext> const &_Handle) noexcept
	{
		mp_pCoroutineContext = &_Handle.promise();

		return false;
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	inline_always TCAsyncDestroy<t_FDestroyFinal> TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::await_resume() noexcept
	{
#if DMibEnableSafeCheck > 0
		mp_bAwaited = true;
#endif
		DMibFastCheck(mp_pCoroutineContext);
		return {fg_Move(mp_fDestroy), fg_Exchange(mp_pCoroutineContext, nullptr)};
	}

	template <typename tf_FDestroy>
	auto fg_AsyncDestroyByValue(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy() > fg_DiscardResult();
		}
	{
		using FDestroy = typename NTraits::TCRemoveReferenceAndQualifiers<tf_FDestroy>::CType;
		return TCAsyncDestroyAwaiter<FDestroy, FDestroy>(fg_Forward<tf_FDestroy>(_fDestroy));
	}

	template <typename tf_FDestroy>
	auto fg_AsyncDestroy(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy() > fg_DiscardResult();
		}
	{
		using FDestroy = typename NTraits::TCRemoveReferenceAndQualifiers<tf_FDestroy>::CType;
		return TCAsyncDestroyAwaiter<FDestroy &, FDestroy>(fg_Forward<tf_FDestroy>(_fDestroy));
	}

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroy(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy() > fg_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = &_ToDestroy]() -> TCFuture<void>
				{
					auto ToDestroy = fg_Move(*pToDestroy);
					co_await fg_Move(ToDestroy).f_Destroy();

					co_return {};
				}
			)
		;
	}

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroy(tf_CToCleanup &&_pToDestroy)
		requires requires
		{
			_pToDestroy->f_Destroy() > fg_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = _pToDestroy]() -> TCFuture<void>
				{
					co_await pToDestroy->f_Destroy();

					co_return {};
				}
			)
		;
	}

	template <typename tf_CActor>
	auto fg_AsyncDestroy(TCActor<tf_CActor> &_ToDestroy)
	{
		return TCAsyncDestroyAwaiter<TCActor<tf_CActor> &, TCActor<tf_CActor> &>(_ToDestroy);
	}

	template <typename tf_CActor>
	auto fg_AsyncDestroyByValue(TCActor<tf_CActor> &_ToDestroy)
	{
		return TCAsyncDestroyAwaiter<TCActor<tf_CActor>, TCActor<tf_CActor>>(_ToDestroy);
	}

	template <typename tf_CToCleanup>
	CAsyncDestroyAwaiter fg_AsyncDestroyGeneric(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy() > fg_DiscardResult();
		}
	{
		return fg_AsyncDestroyGeneric
			(
				[pToDestroy = &_ToDestroy]() -> TCFuture<void>
				{
					auto ToDestroy = fg_Move(*pToDestroy);
					co_await fg_Move(ToDestroy).f_Destroy();

					co_return {};
				}
			)
		;
	}
}
