// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	enum ECoroutineAwaiterResult
	{
		ECoroutineAwaiterResult_None = 0
		, ECoroutineAwaiterResult_Observed = DMibBit(0)
		, ECoroutineAwaiterResult_Set = DMibBit(1)
	};

	template <typename tf_FExceptionTransform>
	auto fg_TransformException(NException::CExceptionPointer &&_pException, tf_FExceptionTransform &_fExceptionTransform)
	{
		NException::CDisableExceptionTraceScope DisableExceptionTrace;
		if constexpr (NTraits::cIsSame<tf_FExceptionTransform, CVoidTag>)
			return fg_Move(_pException);
		else
			return _fExceptionTransform(fg_Move(_pException));
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	template <typename tf_CRight>
	auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::operator > (tf_CRight &&_Right) &&
	{
		return fg_Move(*this).f_Dispatch() > fg_Forward<tf_CRight>(_Right);
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::TCFuturePackAwaiter(NStorage::TCTuple<tp_CFutures...> &&_Futures, t_FExceptionTransform &&_fExceptionTransform)
		: mp_Futures(fg_Move(_Futures))
		, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
	{
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::~TCFuturePackAwaiter()
	{
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	template
	<
		typename tf_CResultFunctor
		, umint... tfp_Indices
	>
	void TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::fp_ActorCall
		(
			tf_CResultFunctor &&_ResultFunctor
			, TCActor<> &&_ResultActor
			, NMeta::TCIndices<tfp_Indices...>
		)
	{
		using CTypeList = NMeta::TCTypeList<typename tp_CFutures::CValue...>;
		using CStorage = NPrivate::TCCallMutipleActorStorage<false, tf_CResultFunctor, TCActor<>, CTypeList>;

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(fg_Move(_ResultActor), fg_Forward<tf_CResultFunctor>(_ResultFunctor));

		(
			[&]
			{
				fg_Move(fg_Get<tfp_Indices>(mp_Futures)).f_OnResultSet
					(
						[pStorage](TCAsyncResult<typename tp_CFutures::CValue> &&_Result)
						{
							auto &Internal = *pStorage;
							fg_Get<tfp_Indices>(Internal.m_Results) = fg_Move(_Result);
							Internal.f_Finished();
						}
					)
				;
			}
			()
			, ...
		);
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	template <umint... tfp_Indices>
	auto TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::fp_Unwrap(NMeta::TCIndices<tfp_Indices...>) -> CUnwrappedType
	{
		return {fg_Move(*fg_Get<tfp_Indices>(mp_Results))...};
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	template <umint... tfp_Indices>
	NException::CExceptionPointer TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::fp_HasException(NMeta::TCIndices<tfp_Indices...>)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;
		(
			... || [&]
			{
				auto &SourceResult = fg_Get<tfp_Indices>(mp_Results);
				if (!SourceResult)
				{
					bSuccess = false;
					ErrorCollector.f_AddError(SourceResult.f_GetException());
				}

				return false;
			}
			()
		);

		if (bSuccess)
			return {};
		else
			return fg_Move(ErrorCollector).f_GetException();
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	template <umint... tfp_Indices>
	void TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::fp_TransformExceptions(CWrappedType &o_Results, NMeta::TCIndices<tfp_Indices...>)
	{
		(
			[&]
			{
				auto &Result = fg_Get<tfp_Indices>(o_Results);
				if (!Result)
					Result.f_SetException(mp_fExceptionTransform(Result.f_GetException()));
			}
			()
			, ...
		);
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	bool TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::await_ready() const noexcept
	{
		return false;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	template <typename tf_CCoroutineContext>
	bool TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
	{
		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;

#if DMibEnableSafeCheck > 0
		auto CurrentActor = fg_CurrentActor(ConcurrencyThreadLocal);
		DMibFastCheck(CurrentActor);
#endif
		auto &CoroutineContext = _Handle.promise();

		fp_ActorCall
			(
				[=, this, KeepAlive = CoroutineContext.f_KeepAliveImplicit()](CWrappedType &&_Results) mutable
				{
					if (!KeepAlive.f_HasValidCoroutine())
						return; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

					auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();

					auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
					auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;

#if DMibEnableSafeCheck > 0
					auto CurrentActor = fg_CurrentActor(ConcurrencyThreadLocal);
					DMibFastCheck(CurrentActor);
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
#endif
					mp_Results = fg_Move(_Results);

					if (!(mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Set) & ECoroutineAwaiterResult_Observed))
						return;

					auto &CoroutineContext = Handle.promise();

					if constexpr (t_bUnwrap)
					{
						if (auto pException = fp_HasException(NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>()))
						{
							NStorage::TCUniquePointer<COnResumeException> pResult = fg_Construct(fg_TransformException(fg_Move(pException), mp_fExceptionTransform));
							CoroutineContext.f_HandleAwaitedResult
								(
									ConcurrencyThreadLocal
									, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor
									, fg_Move(pResult)
								)
							;
							return;
						}
					}

					{
						bool bAborted = false;
						auto RestoreStates = CoroutineContext.f_Resume(ThreadLocal, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor, bAborted);
						if (!bAborted)
							Handle.resume();
					}
				}
				, fg_CurrentActor(ConcurrencyThreadLocal)
				, NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>()
			)
		;

		if (mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Observed) & ECoroutineAwaiterResult_Set)
		{
			if constexpr (t_bUnwrap)
			{
				if (auto pException = fp_HasException(NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>()))
				{
					NStorage::TCUniquePointer<COnResumeException> pResult = fg_Construct(fg_TransformException(fg_Move(pException), mp_fExceptionTransform));
					CoroutineContext.f_HandleAwaitedResult
						(
							ConcurrencyThreadLocal
							, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor
							, fg_Move(pResult)
						)
					;
					return true;
				}
			}

			return false;
		}

		CoroutineContext.f_Suspend(ThreadLocal, true);
		return true;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	auto TCFuturePackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CFutures...>::await_resume() noexcept(!t_bUnwrap)
	{
		if constexpr (t_bUnwrap)
			return fp_Unwrap(NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());
		else
		{
			if constexpr (!NTraits::cIsSame<t_FExceptionTransform, CVoidTag>)
				fp_TransformExceptions(mp_Results, NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());
			return fg_Move(mp_Results);
		}
	}

	template <typename t_CFuturePack, typename ...tp_CFutures>
	TCFuturePackWithError<t_CFuturePack, tp_CFutures...>::TCFuturePackWithError(t_CFuturePack *_pFuturePack, FExceptionTransformer &&_fTransfomer)
		: CActorWithErrorBase(fg_Move(_fTransfomer))
		, mp_pFuturePack{_pFuturePack}
	{
	}

	template <typename t_CFuturePack, typename ...tp_CFutures>
	auto TCFuturePackWithError<t_CFuturePack, tp_CFutures...>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CFuturePack, typename ...tp_CFutures>
	auto TCFuturePackWithError<t_CFuturePack, tp_CFutures...>::operator co_await() &&
	{
		return TCFuturePackAwaiter
			<
				true
				, FExceptionTransformer
				, tp_CFutures...
			>
			{fg_Move(mp_pFuturePack->m_Futures), fg_Move(*this).f_GetTransformer()}
		;
	}

	template <typename t_CFuturePack, typename ...tp_CFutures>
	auto TCFuturePackWithError<t_CFuturePack, tp_CFutures...>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFuturePackAwaiter
			<
				false
				, FExceptionTransformer
				, tp_CFutures...
			>
			{fg_Move(m_pWrapped->mp_pFuturePack->m_Futures), fg_Move(*m_pWrapped).f_GetTransformer()}
		;
	}
}
