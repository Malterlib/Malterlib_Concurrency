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
	TCAsyncGeneratorData<t_CReturnType>::TCAsyncGeneratorData(TFAsyncGeneratorGetNext<t_CReturnType> &&_fGetNext, bool _bSupportsPipelines, bool _bIsCoroutine)
		: TCAsyncGeneratorDataShared<t_CReturnType>{.m_fGetNext{fg_Move(_fGetNext)}}
		, m_bSupportsPipelines(_bSupportsPipelines)
		, m_bIsCoroutine(_bIsCoroutine)
	{
	}

	template <typename t_CReturnType>
	TCAsyncGeneratorDataPipelined<t_CReturnType>::TCAsyncGeneratorDataPipelined(TCAsyncGeneratorData<t_CReturnType> &&_Other)
		: TCAsyncGeneratorDataShared<t_CReturnType>{.m_fGetNext = fg_Move(_Other.m_fGetNext)}
		, m_bSupportsPipelines(_Other.m_bSupportsPipelines)
		, m_bIsCoroutine(_Other.m_bIsCoroutine)
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
		using CReturnNoReference = NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>;

		if constexpr (NException::cIsException<CReturnNoReference>)
		{
			static_assert
				(
					(NTraits::cIsRValueReference<tf_CReturnType> || !NTraits::cIsReference<tf_CReturnType>)
					&& !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CReturnType>>
					, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::cIsSame<CReturnNoReference, NException::CExceptionPointer>)
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		else
			static_assert(NTraits::cIsVoid<tf_CReturnType>, "Can only return exceptions from generators");
#endif
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(tf_CReturnType &&_Value) -> CYieldSuspender
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		using CReturnNoReference = NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>;
		if constexpr (NTraits::cIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>)
		{
			pPromiseData->m_Result.f_SetResult(fg_Forward<tf_CReturnType>(_Value));
			pPromiseData->f_OnResultNoClear();
		}
		else if constexpr (NException::cIsException<CReturnNoReference>)
			static_assert(NTraits::cIsVoid<tf_CReturnType>, "Exceptions cannot be yielded, use co_return instead");
		else if constexpr (NTraits::cIsSame<CReturnNoReference, NException::CExceptionPointer>)
			static_assert(NTraits::cIsVoid<tf_CReturnType>, "Exceptions cannot be yielded, use co_return instead");
		else
		{
			pPromiseData->m_Result.f_SetResult(fg_Forward<tf_CReturnType>(_Value));
			pPromiseData->f_OnResultNoClear();
		}
#endif
		return {};
	}

	template <typename t_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(t_CReturnType &&_Value) -> CYieldSuspender
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<NStorage::TCOptional<t_CReturnType>> *>(this->m_pPromiseData);
		pPromiseData->m_Result.f_SetResult(fg_Move(_Value));
		pPromiseData->f_OnResultNoClear();
#endif
		return {};
	}

	template <typename t_CReturnType>
	CSuspendAlways TCAsyncGeneratorCoroutineContext<t_CReturnType>::initial_suspend() noexcept
	{
		auto &ThreadLocal = fg_SystemThreadLocal();

		this->m_bAwaitingInitialResume = true;

		if (!ThreadLocal.m_CrossActorStateScopes.f_IsEmpty()) [[unlikely]]
		{
			for (auto &Scope : ThreadLocal.m_CrossActorStateScopes)
				Scope.f_InitialSuspend();
		}

		this->f_InitialSuspend(ThreadLocal);

		return {};
	}

	template <typename t_CReturnType>
	TCUnsafeFuture<NStorage::TCOptional<t_CReturnType>> TCAsyncGeneratorCoroutineContext<t_CReturnType>::CGeneratorRunState::f_GetGeneratorValue()
	{
		m_Lock.f_Lock();
		auto Cleanup = g_OnScopeExit / [&]
			{
				m_Lock.f_Unlock();
			}
		;

		if (m_bDone || m_bDestroyed || !m_pPromiseData)
			co_return {};

		if (m_pException)
			co_return m_pException;

		TCPromiseFuturePair<NStorage::TCOptional<t_CReturnType>> Promise;
		m_QueuedCalls.f_Insert(fg_Move(Promise.m_Promise));

		DMibLog(DMibAsyncGeneratorDebugLevel, "f_GetGeneratorValue {} Queued: {}", fg_GetTypeName<t_CReturnType>(), m_QueuedCalls.f_GetLen());

		if (!fg_Exchange(m_bInProgress, true))
			f_RunGenerator();

		Cleanup();

		co_return co_await fg_Move(Promise.m_Future);

		co_return {};
	}

	template <typename t_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::fs_KeepaliveSetResultFunctor(CFutureCoroutineContext &_Context, TCActor<CActor> &&_Actor)
		-> CFutureCoroutineContext::FDeliverResult
	{
		auto *pThis = static_cast<TCAsyncGeneratorCoroutineContext<t_CReturnType> *>(&_Context);

		return [KeepAlive = pThis->f_KeepAlive(fg_Move(_Actor))](NStorage::TCUniquePointer<ICOnResumeResult> &&_pResult)
			{
				if (!_pResult)
					return;  // Should not happen

				if (auto pException = fg_Move(*_pResult).f_TryGetException())
					KeepAlive.f_PromiseData().f_SetException(fg_Move(pException));
				else
					_pResult->f_ApplyValue(&KeepAlive.f_PromiseData());
			}
		;
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::CGeneratorRunState::f_RunGenerator()
	{
		DMibFastCheck(m_pPromiseData);

		auto &PromiseData = *m_pPromiseData;

		m_RefCount.m_RefCount.f_FetchAdd(2, NAtomic::gc_MemoryOrder_Relaxed);

		DMibFastCheck(!m_bDestroyed);

		PromiseData.f_Reset
			(
				[pRunState = NStorage::TCSharedPointer<CGeneratorRunState>(fg_Attach(this))]
				(TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Result) mutable
				{
					auto pRunStateLocal = fg_Move(pRunState);

					auto &RunState = *pRunStateLocal;
					DMibLock(RunState.m_Lock);

					if (RunState.m_QueuedCalls.f_IsEmpty())
						return;

					if (!_Result)
					{
						RunState.m_pException = _Result.f_GetException();
						auto Calls = fg_Move(RunState.m_QueuedCalls);

						for (auto &Promise : Calls)
							Promise.f_SetException(RunState.m_pException);

						return;
					}

					if (!*_Result)
					{
						RunState.m_bDone = true;
						auto Calls = fg_Move(RunState.m_QueuedCalls);
						for (auto &Promise : Calls)
							Promise.f_SetResult(NStorage::TCOptional<t_CReturnType>());

						if (RunState.m_pPromiseData)
							RunState.m_pPromiseData->m_fOnResult.f_Clear();
					}
					else
					{
						RunState.m_QueuedCalls.f_PopFirst().f_SetResult(fg_Move(_Result));

						DMibLog(DMibAsyncGeneratorDebugLevel, "Deliver value {} Queued: {}", fg_GetTypeName<t_CReturnType>(), RunState.m_QueuedCalls.f_GetLen());

						if (RunState.m_QueuedCalls.f_IsEmpty())
						{
							RunState.m_bInProgress = false;
							if (RunState.m_pPromiseData)
								RunState.m_pPromiseData->m_fOnResult.f_Clear();
						}
						else if (RunState.m_pPromiseData)
							RunState.f_RunGenerator();
					}
				}
			)
		;

		fg_Dispatch
			(
				m_AsyncGeneratorOwner
				, [KeepAlive = TCFutureCoroutineKeepAliveImplicit<NStorage::TCOptional<t_CReturnType>>(fg_Explicit(m_pPromiseData))]() mutable -> TCFuture<void>
				{
					if (!KeepAlive.f_HasValidCoroutine())
						co_return {}; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

					auto &ThreadLocal = fg_SystemThreadLocal();

 #if DMibEnableSafeCheck > 0
					auto CurrentActor = fg_CurrentActor(ThreadLocal.m_PromiseThreadLocal.f_ConcurrencyThreadLocal());
					DMibFastCheck(CurrentActor);
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
 #endif
					auto CoroutineHandle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();
					auto &CoroutineContext = CoroutineHandle.promise();

					if (CoroutineContext.m_bAwaitingInitialResume)
					{
						CoroutineContext.m_bAwaitingInitialResume = false;

						{
							FRestoreScopesState RestoreScopes;
							CoroutineContext.f_ResumeInitialSuspend(ThreadLocal, RestoreScopes);

							CoroutineHandle.resume();
						}
					}
					else
					{
						bool bAborted = false;
						auto RestoreStates = CoroutineContext.f_Resume
							(
								ThreadLocal
								, &TCAsyncGeneratorCoroutineContext<NStorage::TCOptional<t_CReturnType>>::fs_KeepaliveSetResultFunctor
								, bAborted
							)
						;

						if (!bAborted)
							CoroutineHandle.resume();
					}

					co_return {};
				}
			)
			.f_OnResultSet
			(
				[pRunState = NStorage::TCSharedPointer<CGeneratorRunState>(fg_Attach(this))](TCAsyncResult<void> &&_Result)
				{
					if (_Result)
						return;

					auto &RunState = *pRunState;

					DMibLock(RunState.m_Lock);

					RunState.m_pException = _Result.f_GetException();

					auto Calls = fg_Move(RunState.m_QueuedCalls);

					for (auto &Promise : Calls)
						Promise.f_SetException(RunState.m_pException);
				}
			)
		;
	}

	template <typename t_CReturnType>
	struct TCAsyncGeneratorCoroutineFunctor
	{
		NStorage::TCSharedPointer<typename TCAsyncGeneratorCoroutineContext<t_CReturnType>::CGeneratorRunState> m_pRunState;
		TCFutureCoroutineKeepAliveImplicit<NStorage::TCOptional<t_CReturnType>> m_KeepAlive;
#ifndef DCompiler_MSVC_Workaround
		DMibNoUniqueAddress
#endif
		CAllowWrongThreadDestroy m_AllowDestroy;

		void f_Destroy();

		TCFuture<NStorage::TCOptional<t_CReturnType>> operator ()()
		{
			return m_pRunState->f_GetGeneratorValue();
		}
	};

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineFunctor<t_CReturnType>::f_Destroy()
	{
		m_pRunState->f_Destroy();
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::CGeneratorRunState::f_Destroy()
	{
		DMibLock(m_Lock);
		m_QueuedCalls.f_Clear();
		m_bDestroyed = true;
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorDataShared<t_CReturnType>::f_Destroy(bool _bIsCoroutine)
	{
		if (!_bIsCoroutine || m_fGetNext.f_IsEmpty())
			return;

		auto pFunctor = static_cast<NPrivate::TCAsyncGeneratorCoroutineFunctor<t_CReturnType> *>(m_fGetNext.f_GetFunctor().f_GetFunctor());
		pFunctor->f_Destroy();
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGeneratorDataShared<t_CReturnType>::fs_AsyncDestroy
		(
			NStorage::TCSharedPointer<typename TCAsyncGeneratorCoroutineContext<t_CReturnType>::CGeneratorRunState> _pRunState
		)
	{
		auto &RunState = *_pRunState;

		TCActor<CActor> OwnerActor;
		{
			DMibLock(RunState.m_Lock);

			OwnerActor = RunState.m_AsyncGeneratorOwner.f_Lock();
			if (!OwnerActor)
			{
				co_return {};
			}
		}

		if (fg_CurrentActor() != OwnerActor)
		{
			TCPromiseFuturePair<void> Promise;

			co_await fg_ContinueRunningOnActor(OwnerActor);
		}

		auto pPromiseData = RunState.m_pPromiseData;
		if (!pPromiseData || !pPromiseData->m_Coroutine)
			co_return {};

		auto &CoroutineContext = pPromiseData->m_Coroutine.promise();
		auto Future = CoroutineContext.f_AsyncDestroy(fg_ConcurrencyThreadLocal());

		if (Future.f_IsValid())
		{
			if (Future.f_ObserveIfAvailable())
				co_return {};

			co_await fg_ContinueRunningOnActor(fg_ThisConcurrentActor());
			co_await fg_Move(Future);
		}

		co_return {};
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGeneratorDataShared<t_CReturnType>::f_AsyncDestroy(bool _bIsCoroutine)
	{
		if (!_bIsCoroutine || !m_fGetNext)
			return g_Void;

		auto pFunctor = static_cast<NPrivate::TCAsyncGeneratorCoroutineFunctor<t_CReturnType> *>(m_fGetNext.f_GetFunctor().f_GetFunctor());

		return fs_AsyncDestroy(pFunctor->m_pRunState);
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGeneratorDataPipelined<t_CReturnType>::f_AsyncDestroy()
	{
		return TCAsyncGeneratorDataShared<t_CReturnType>::f_AsyncDestroy(m_bIsCoroutine);
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGeneratorData<t_CReturnType>::f_AsyncDestroy()
	{
		return TCAsyncGeneratorDataShared<t_CReturnType>::f_AsyncDestroy(m_bIsCoroutine);
	}

	template <typename t_CReturnType>
	TCAsyncGeneratorCoroutineContext<t_CReturnType>::~TCAsyncGeneratorCoroutineContext()
	{
		if (m_pRunState)
		{
			auto &RunState = *m_pRunState;

			NTime::CStopwatch Stopwatch{true};
			RunState.m_Lock.f_Lock();
			auto LockCleanup = g_OnScopeExit / [&]
				{
					RunState.m_Lock.f_Unlock();
				}
			;

			RunState.m_pPromiseData = nullptr;
		}
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType> TCAsyncGeneratorCoroutineContext<t_CReturnType>::get_return_object()
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		auto pPromiseData = this->fp_GetReturnObjectGenerator(PromiseThreadLocal);
		auto *pData = pPromiseData.f_Get();
		pData->m_BeforeSuspend.m_bIsGenerator = true;
		pData->m_BeforeSuspend.m_bOnResultSetAtInit = false;
		pData->m_BeforeSuspend.f_NeedAtomicRead();

		TCActor<CActor> CurrentActor;

		if (PromiseThreadLocal.m_pCurrentActorCalled)
			CurrentActor = fg_ThisActor((TCActorInternal<CActor> *)PromiseThreadLocal.m_pCurrentActorCalled);
		else
			CurrentActor = fg_CurrentActor(ConcurrencyThreadLocal);

		m_pRunState = fg_Construct(CurrentActor);
		m_pRunState->m_pPromiseData = pData;

#if DMibEnableSafeCheck > 0
		if (PromiseThreadLocal.m_pOnResultSetConsumedBy == this)
			PromiseThreadLocal.m_pOnResultSetConsumedBy = pData;
#endif
		return
			{
				g_ActorFunctor(CurrentActor)
				(
					g_ActorSubscription(fg_DirectCallActor()) / [pRunState = m_pRunState]()
					{
						pRunState->m_bAborted.f_Exchange(true);
					}
				)
				/ TCAsyncGeneratorCoroutineFunctor<t_CReturnType>{.m_pRunState = m_pRunState, .m_KeepAlive{fg_Move(pPromiseData)}}
				, true
				, true
			}
		;
	}
}
