// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_FDestroy>
	template <typename tf_FDestroy>
	TCAsyncDestroy<t_FDestroy>::TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
		requires (!NTraits::cIsReference<t_FDestroy>)
		: mp_fDestroy(fg_Forward<tf_FDestroy>(_fDestroy))
		, mp_pCoroutineContext(_pCoroutineContext)
	{
	}

	template <typename t_FDestroy>
	template <typename tf_FDestroy>
	TCAsyncDestroy<t_FDestroy>::TCAsyncDestroy(tf_FDestroy &&_fDestroy, CFutureCoroutineContext *_pCoroutineContext)
		requires (NTraits::cIsReference<t_FDestroy>)
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

		DMibFastCheck(pCoroContext->m_bRuntimeStateConstructed);

		if constexpr (NTraits::cIsReference<t_FDestroy>)
			pCoroContext->m_State.m_AsyncDestructors.f_Insert(fg_Move(mp_fDestroy).f_Destroy());
		else
			pCoroContext->m_State.m_AsyncDestructors.f_Insert(fg_CallSafe(fg_Move(mp_fDestroy)));
	}

	template <typename t_FDestroy>
	void TCAsyncDestroy<t_FDestroy>::f_Clear()
	{
		mp_pCoroutineContext = nullptr;
	}

	template <typename t_FDestroy>
	TCFutureAwaiter<void, false> TCAsyncDestroy<t_FDestroy>::CNoUnwrap::operator co_await () &&
	{
		DMibFastCheck(m_pWrapped->mp_pCoroutineContext);
		m_pWrapped->mp_pCoroutineContext = nullptr;
		if constexpr (NTraits::cIsReference<t_FDestroy>)
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
	TCFutureAwaiter<void, true> TCAsyncDestroy<t_FDestroy>::operator co_await () &&
	{
		DMibFastCheck(mp_pCoroutineContext);
		mp_pCoroutineContext = nullptr;
		if constexpr (NTraits::cIsReference<t_FDestroy>)
			return fg_Move(mp_fDestroy).f_Destroy();
		else
			return {fg_CallSafe(fg_Move(mp_fDestroy)), {}};
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	template <typename tf_FDestroy>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
		requires (!NTraits::cIsReference<t_FDestroy>)
		: mp_fDestroy(fg_Forward<tf_FDestroy>(_fDestroy))
	{
	}

	template <typename t_FDestroy, typename t_FDestroyFinal>
	template <typename tf_FDestroy>
	TCAsyncDestroyAwaiter<t_FDestroy, t_FDestroyFinal>::TCAsyncDestroyAwaiter(tf_FDestroy &&_fDestroy)
		requires (NTraits::cIsReference<t_FDestroy>)
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

	namespace NPrivate
	{
		void fg_AsyncDestroyLogErrorHelper(CAsyncResult const &_Result);
	}

	template <typename tf_FDestroy>
	auto fg_AsyncDestroyByValue(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	{
		using FDestroy = NTraits::TCRemoveReferenceAndQualifiers<tf_FDestroy>;
		return TCAsyncDestroyAwaiter<FDestroy, FDestroy>(fg_Forward<tf_FDestroy>(_fDestroy));
	}

	template <typename tf_FDestroy>
	auto fg_AsyncDestroy(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	{
		using FDestroy = NTraits::TCRemoveReferenceAndQualifiers<tf_FDestroy>;
		return TCAsyncDestroyAwaiter<FDestroy &, FDestroy>(fg_Forward<tf_FDestroy>(_fDestroy));
	}

	template <typename tf_FDestroy>
	auto fg_AsyncDestroyLogError(tf_FDestroy &&_fDestroy)
		requires requires
		{
			_fDestroy().f_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[fDestroy = fg_Forward<tf_FDestroy>(_fDestroy)]() mutable -> TCFuture<void>
				{
					NPrivate::fg_AsyncDestroyLogErrorHelper(co_await fg_CallSafe(fg_Move(fDestroy)).f_Wrap());

					co_return {};
				}
			)
		;
	}

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroy(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy().f_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = &_ToDestroy]() -> TCUnsafeFuture<void>
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
			fg_Move(*_pToDestroy).f_Destroy().f_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = fg_Forward<tf_CToCleanup>(_pToDestroy)]() mutable -> TCFuture<void>
				{
					auto pToDestroyCoroutine = fg_Move(pToDestroy);
					co_await fg_Move(*pToDestroyCoroutine).f_Destroy();

					co_return {};
				}
			)
		;
	}

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroyLogError(tf_CToCleanup &_ToDestroy)
		requires requires
		{
			fg_Move(_ToDestroy).f_Destroy().f_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = &_ToDestroy]() -> TCUnsafeFuture<void>
				{
					auto ToDestroy = fg_Move(*pToDestroy);
					NPrivate::fg_AsyncDestroyLogErrorHelper(co_await fg_Move(ToDestroy).f_Destroy().f_Wrap());

					co_return {};
				}
			)
		;
	}

	template <typename tf_CToCleanup>
	auto fg_AsyncDestroyLogError(tf_CToCleanup &&_pToDestroy)
		requires requires
		{
			fg_Move(*_pToDestroy).f_Destroy().f_DiscardResult();
		}
	{
		return fg_AsyncDestroyByValue
			(
				[pToDestroy = _pToDestroy]() -> TCFuture<void>
				{
					auto pToDestroyCoroutine = fg_Move(pToDestroy);
					NPrivate::fg_AsyncDestroyLogErrorHelper(co_await fg_Move(*pToDestroyCoroutine)->f_Destroy().f_Wrap());

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

	// Waits, when the coroutine returns, for a call on _Actor, so that what the coroutine dispatched to the actor and awaited is over,
	// its captures destroyed, before its result resumes the caller: the actor destroys a dispatched function and its captures after
	// the function's result has already resumed the coroutine. It doesn't wait for calls that others queue on the actor, nor for a
	// dispatched coroutine that moves on to another actor, as with fg_ContinueRunningOnActor, which destroys its captures there.
	// _Actor is read when the coroutine returns, so it must outlive the returned async destroy and still name the same actor. A
	// sham actor, as fg_ThisConcurrentActor(), picks a real actor for each call, and a direct call actor runs calls on the caller's
	// thread, so no call on them waits for an earlier one: they are refused by their holder type, which needn't be complete here, and
	// with a fast check for a type erased actor, one delegated to them, or one whose holder derives from theirs.
	template <typename tf_CActor>
	auto fg_SynchronizeOnExit(TCActor<tf_CActor> &_Actor)
		requires (!NTraits::cIsSame<typename tf_CActor::CActorHolder, CShamActorHolder> && !NTraits::cIsSame<typename tf_CActor::CActorHolder, CDirectCallActorHolder>)
	{
		return fg_AsyncDestroyByValue
			(
				[pActor = &_Actor]() -> TCFuture<void>
				{
					return NPrivate::fg_SynchronizeWithActor(*pActor);
				}
			)
		;
	}

	// Also releases the checkout, only after the synchronization, so that no other checkout queues work on the actor before it
	inline auto fg_SynchronizeOnExit(CBlockingActorCheckout &_Checkout)
	{
		return fg_AsyncDestroy(_Checkout);
	}
}
