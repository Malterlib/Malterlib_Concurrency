// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ActorSubscription>

namespace NMib::NConcurrency
{
	template <typename tf_CReturnType>
	auto begin(TCAsyncGenerator<tf_CReturnType> &_Generator)
	{
		return fg_Move(_Generator).f_GetIterator();
	}

	template <typename tf_CReturnType>
	auto end(TCAsyncGenerator<tf_CReturnType> &_Generator) -> NContainer::CIteratorEndSentinel
	{
		return NContainer::CIteratorEndSentinel();
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnType>
	TCAsyncGeneratorData<t_CReturnType>::TCAsyncGeneratorData(TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> &&_fGetNext)
		: m_fGetNext(fg_Move(_fGetNext))
	{
	}

	template <typename t_CReturnType>
	TCAsyncGeneratorCoroutineContext<t_CReturnType>::TCAsyncGeneratorCoroutineContext(ECoroutineFlag _Flags) noexcept
		: TCFutureCoroutineContextShared<NStorage::TCOptional<t_CReturnType>>(_Flags)
	{
	}

	template <typename t_CReturnType>
	bool TCAsyncGeneratorCoroutineContext<t_CReturnType>::CYieldSuspender::await_ready() const noexcept
	{
		return false;
	}

	template <typename t_CReturnType>
	template <typename tf_CPromise>
	inline bool TCAsyncGeneratorCoroutineContext<t_CReturnType>::CYieldSuspender::await_suspend(TCCoroutineHandle<tf_CPromise> _Producer) noexcept
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
#if DMibEnableSafeCheck > 0
		auto CurrentActor = fg_CurrentActor(ThreadLocal.m_PromiseThreadLocal.f_ConcurrencyThreadLocal());
		DMibFastCheck(CurrentActor);
#endif

		auto &CoroutineContext = _Producer.promise();
		CoroutineContext.f_Suspend(ThreadLocal, true);

		return true;
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::CYieldSuspender::await_resume() noexcept
	{
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::return_value(CEmpty)
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		pPromiseData->f_SetResultNoReport(NStorage::TCOptional<t_CReturnType>{});
#endif
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;

		if constexpr (NException::TCIsException<CReturnNoReference>::mc_Value)
		{
			static_assert
				(
					(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
					&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
					, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		else
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Can only return exceptions from generators");
#endif
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(tf_CReturnType &&_Value) -> CYieldSuspender
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsException<CReturnNoReference>::mc_Value)
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Exceptions cannot be yielded, use co_return instead");
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Exceptions cannot be yielded, use co_return instead");
		else
			pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
#endif
		return {};
	}

	template <typename t_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(t_CReturnType &&_Value) -> CYieldSuspender
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		pPromiseData->f_SetResult(fg_Move(_Value));
#endif
		return {};
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType> TCAsyncGeneratorCoroutineContext<t_CReturnType>::get_return_object()
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		auto pPromiseData = this->fp_GetReturnObjectGenerator(PromiseThreadLocal);
		pPromiseData->m_BeforeSuspend.m_bIsGenerator = true;
		pPromiseData->m_BeforeSuspend.m_bOnResultSetAtInit = false;
		pPromiseData->m_BeforeSuspend.f_NeedAtomicRead();
		pPromiseData->m_BeforeSuspend.m_bNeedToRefcountPromise = true;

		TCActor<CActor> CurrentActor;

		if (PromiseThreadLocal.m_pCurrentActorCalled)
			CurrentActor = fg_ThisActor((TCActorInternal<CActor> * )PromiseThreadLocal.m_pCurrentActorCalled);
		else
			CurrentActor = fg_CurrentActor(ConcurrencyThreadLocal);

		m_AsyncGeneratorOwner = CurrentActor;
		m_pAborted = fg_Construct(false);

#if DMibEnableSafeCheck > 0
		if (PromiseThreadLocal.m_pOnResultSetConsumedBy == this)
			PromiseThreadLocal.m_pOnResultSetConsumedBy = pPromiseData.f_Get();
#endif
		return g_ActorFunctor(CurrentActor)
			(
				g_ActorSubscription(fg_DirectCallActor()) / [pAborted = m_pAborted]()
				{
					pAborted->f_Exchange(true);
				}
			)
			/
			[
				pPromiseData = fg_Move(pPromiseData)
				, bInProgress = false
				, bInitialSuspend = true
				, pAborted = m_pAborted
				, AllowDestroy = g_AllowWrongThreadDestroy
			]() mutable -> TCFuture<NStorage::TCOptional<t_CReturnType>>
			{
				if (fg_Exchange(bInProgress, true))
					co_return DMibErrorInstance("Iteration already in progress");

				if (bInitialSuspend)
				{
					bInitialSuspend = false;
					pPromiseData->m_Coroutine.promise().CFutureCoroutineContext::initial_suspend();
					pPromiseData->m_Coroutine.resume();
				}

				TCFuture<NStorage::TCOptional<t_CReturnType>> Future(pPromiseData);
				auto Result = co_await fg_Move(Future);
				bInProgress = false;

				if (Result)
				{
					auto &CoroutineContext = TCCoroutineHandle<TCAsyncGeneratorCoroutineContext<t_CReturnType>>::from_address(pPromiseData->m_Coroutine.address()).promise();

					if (CoroutineContext.m_AsyncGeneratorOwner != fg_CurrentActor())
					{
						auto Actor = CoroutineContext.m_AsyncGeneratorOwner.f_Lock();
						if (!Actor)
							co_return DMibErrorInstance("Generator owner was deleted");

						co_await fg_ContinueRunningOnActor(Actor);
					}

					pPromiseData->f_Reset();
					pPromiseData->m_BeforeSuspend.m_bOnResultSetAtInit = false;

					fg_Dispatch
						(
							CoroutineContext.m_AsyncGeneratorOwner
							, [KeepAlivePromise = TCFutureCoroutineKeepAliveImplicit<NStorage::TCOptional<t_CReturnType>>(fg_TempCopy(pPromiseData))]
							() mutable
							{
								if (!KeepAlivePromise.f_HasValidCoroutine())
									return; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

								auto &ThreadLocal = fg_SystemThreadLocal();

			 #if DMibEnableSafeCheck > 0
								auto CurrentActor = fg_CurrentActor(ThreadLocal.m_PromiseThreadLocal.f_ConcurrencyThreadLocal());
								DMibFastCheck(CurrentActor);
								DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
			 #endif
								auto CoroutineHandle = KeepAlivePromise.template f_Coroutine<CFutureCoroutineContext>();
								auto &CoroutineContext = CoroutineHandle.promise();
								bool bAborted = false;
								auto RestoreStates = CoroutineContext.f_Resume
									(
										ThreadLocal
										, &TCFutureCoroutineContextShared<NStorage::TCOptional<t_CReturnType>>::fs_GetVirtualKeepAlive
										, bAborted
									)
								;
								if (!bAborted)
									CoroutineHandle.resume();
							}
						)
						.f_DiscardResult()
					;
				}

				co_return Result;
			}
		;
	}
}
