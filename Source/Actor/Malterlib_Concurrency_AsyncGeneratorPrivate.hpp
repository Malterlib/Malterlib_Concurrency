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
#if DMibEnableSafeCheck > 0
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		DMibFastCheck(fg_CurrentActorProcessingOrOverridden());
#endif

		auto &CoroutineContext = _Producer.promise();
		CoroutineContext.f_Suspend(true);

		return true;
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::CYieldSuspender::await_resume() noexcept
	{
	}

	template <typename t_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::return_value(CEmpty)
	{
		this->m_pPromiseData->f_SetResultNoReport(NStorage::TCOptional<t_CReturnType>{});
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCAsyncGeneratorCoroutineContext<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;

		if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value)
		{
			static_assert
				(
					(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
					&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
					, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			try
			{
				std::rethrow_exception(_Value);
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
			}
			catch (...)
			{
				this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
			}
		}
		else
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Can only return exceptions from generators");
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(tf_CReturnType &&_Value) -> CYieldSuspender
	{
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			this->m_pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value)
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Exceptions cannot be yielded, use co_return instead");
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
			static_assert(NTraits::TCIsVoid<tf_CReturnType>::mc_Value, "Exceptions cannot be yielded, use co_return instead");
		else
			this->m_pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));

		return {};
	}

	template <typename t_CReturnType>
	auto TCAsyncGeneratorCoroutineContext<t_CReturnType>::yield_value(t_CReturnType &&_Value) -> CYieldSuspender
	{
		this->m_pPromiseData->f_SetResult(fg_Move(_Value));

		return {};
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType> TCAsyncGeneratorCoroutineContext<t_CReturnType>::get_return_object()
	{
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<void (TCAsyncResult<TCAsyncGenerator<t_CReturnType>> &&_AsyncResult)>> pKeepAliveParams =
			fg_Construct
			(
				fg_ConsumeFutureOnResultSet<TCAsyncGenerator<t_CReturnType>>
				(
	#if DMibEnableSafeCheck > 0
					this
					, EConsumeFutureOnResultSetFlag_None
	#endif
				)
			)
		;

		auto pPromiseData = this->fp_GetReturnObject();
		pPromiseData->m_bIsGenerator = true;
		pPromiseData->m_bOnResultSetAtInit = false;

#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = **g_SystemThreadLocal;
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;
		if (PromiseThreadLocal.m_pOnResultSetConsumedBy == this)
			PromiseThreadLocal.m_pOnResultSetConsumedBy = pPromiseData.f_Get();
		auto CreatedOnActor = fg_CurrentActor();
#endif
		return g_ActorFunctor
			(
				g_ActorSubscription / []
				{
				}
			)
			/
			[
				pKeepAliveParams = fg_Move(pKeepAliveParams)
				, pPromiseData = fg_Move(pPromiseData)
				, bInProgress = false
#if DMibEnableSafeCheck > 0
				, CreatedOnActor
#endif
			]() mutable -> TCFuture<NStorage::TCOptional<t_CReturnType>>
			{
				if (bInProgress)
					co_return DMibErrorInstance("Iteration already in progress");

				bInProgress = true;
				TCFuture<NStorage::TCOptional<t_CReturnType>> Future(pPromiseData);
				auto Result = co_await fg_Move(Future);
				bInProgress = false;
				if (Result)
				{
					pPromiseData->f_Reset();
					pPromiseData->m_bOnResultSetAtInit = false;

					g_Dispatch /
						[
							pKeepAliveParams
							, KeepAlivePromise = TCFutureCoroutineKeepAliveImplicit<NStorage::TCOptional<t_CReturnType>>(fg_TempCopy(pPromiseData))
							, CoroutineHandle = pPromiseData->m_Coroutine
#if DMibEnableSafeCheck > 0
							, CreatedOnActor
#endif
						]
						() mutable
						{
		 #if DMibEnableSafeCheck > 0
							if (!KeepAlivePromise.f_HasValidCoroutine())
								return; // Can happen when f_Suspend throws

							auto CurrentActor = fg_CurrentActor();
							DMibFastCheck(CurrentActor == CreatedOnActor);
							DMibFastCheck(CurrentActor);
							DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
		 #endif
							auto &CoroutineContext = CoroutineHandle.promise();
							bool bAborted = false;
							auto RestoreStates = CoroutineContext.f_Resume(bAborted, false);
							if (!bAborted)
								CoroutineHandle.resume();
						}
						> fg_DiscardResult()
					;
				}

				co_return Result;
			}
		;
	}

	template <typename t_CReturnValue, typename t_CException>
	template <typename tf_FToRun, typename ...tfp_CParams>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<TCAsyncGenerator<t_CReturnValue>, t_CException>
		::operator () (tf_FToRun &&_ToRun, tfp_CParams && ...p_Params) const
	{
		TCPromise<t_CReturnValue> Result;
		NException::CDisableExceptionTraceScope DisableTrace;
		if constexpr (NTraits::TCIsVoid<t_CException>::mc_Value)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			try
			{
				Result.f_SetResult(fg_CallSafe(_ToRun, fg_Forward<tfp_CParams>(p_Params)...));
			}
			catch (...)
			{
				Result.f_SetCurrentException();
			}
		}
		else
		{
			try
			{
				Result.f_SetResult(fg_CallSafe(_ToRun, fg_Forward<tfp_CParams>(p_Params)...));
			}
			catch (t_CException const &)
			{
				Result.f_SetCurrentException();
			}
		}
		return Result.f_MoveFuture();
	}

	template <typename t_CReturnValue, typename t_CException>
	template <typename tf_FToRun>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<TCAsyncGenerator<t_CReturnValue>, t_CException>::operator / (tf_FToRun &&_ToRun) const
	{
		return (*this)(fg_Forward<tf_FToRun>(_ToRun));
	}
}
