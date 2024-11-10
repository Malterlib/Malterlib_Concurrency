// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CKey, typename t_CValue, typename t_CForwardKey>
	struct TCFutureMapBoundKey;

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	struct TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>
	{
		using CIndices = typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CParams)>::CType;
		using CValue = t_CReturn;
		using CParamsType = typename TCChooseType
			<
				t_bByValue
				, NStorage::TCTuple<tp_CParams...>
				, NStorage::TCTuple<typename NMib::NTraits::TCAddLValueReference<tp_CParams>::CType...>
			>::CType
		;

		struct CNoUnwrapAsyncResult
		{
			TCBoundActorCallAwaiter<t_CReturn, TCBoundActorCall, false, CVoidTag> operator co_await() &&;

			TCBoundActorCall *m_pWrapped;
		};

		inline_always void operator > (CDiscardResult const &_Discard) &&;
		inline_always void operator > (TCFutureVector<t_CReturn> &_FutureVector) &&;
		inline_always void operator > (TCVectorOfFutures<t_CReturn> &_ResultVector) &&;
		inline_always void operator > (NContainer::TCMapResult<TCFuture<t_CReturn> &> &&_MapResult) &&;

		template <typename tf_CKey, typename tf_CForwardKey>
		inline_always void operator > (TCFutureMapBoundKey<tf_CKey, t_CReturn, tf_CForwardKey> &&_BoundKey) &&;

		template <typename tf_FOnResult>
		inline_always void operator > (tf_FOnResult &&_fOnResult) &&
			requires (NTraits::TCIsCallableWith<tf_FOnResult, void (TCAsyncResult<t_CReturn> &&_Result)>::mc_Value)
		;

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
			requires (NTraits::TCIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<t_CReturn> &&)>::mc_Value) // Incorrect type in result call
		;

		template <typename tf_FOnResult>
		void operator > (TCDirectResultFunctor<tf_FOnResult> &&_fOnResult) &&;

		template <typename tf_CReturnValue>
		inline_always void operator > (TCPromise<tf_CReturnValue> &&_Promise) &&;

		template <typename tf_CResult>
		void operator > (TCPromiseWithExceptionTransfomer<tf_CResult> &&_PromiseWithError) &&;

		auto operator % (NStr::CStr const &_ErrorString) && -> TCFutureWithExceptionTransformer<t_CReturn, TCBoundActorCall>;

		auto f_CallSync() &&;
		auto f_CallSync(fp64 _Timeout) &&;
		auto f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout = -1.0) &&;

		TCFuture<t_CReturn> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true) &&;

		inline_always CNoUnwrapAsyncResult f_Wrap() &&;

		TCBoundActorCallAwaiter<t_CReturn, TCBoundActorCall, true, CVoidTag> operator co_await() &&;

		template <typename tf_CType>
		inline_always auto operator + (TCFuture<tf_CType> &&_Other) &&;

		template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
		inline_always auto operator + (TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other) &&;

		template <typename tf_CReturn, typename tf_CFutureLike>
		inline_always auto operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other) &&;

		template <typename tf_FOnResult>
		inline_always void f_OnResultSet(tf_FOnResult &&_fOnResult) &&;

		inline_always void f_DiscardResult() &&;

		TCFuture<t_CReturn> f_Call() &&;

		operator TCFuture<t_CReturn> () &&;

		t_FFunctionPointer mp_pMemberFunction;
		t_CActor mp_Actor;
		CParamsType mp_Params;

	private:
		template <mint ...tfp_Indices>
		inline_always void fp_CallDiscard(NMeta::TCIndices<tfp_Indices...> _Indices) &&;

		template <mint ...tfp_Indices>
		inline_always TCFuture<t_CReturn> fp_Call(NMeta::TCIndices<tfp_Indices...> _Indices) &&;

		template <typename tf_FOnResult, mint ...tfp_Indices>
		inline_always void fp_CallOnResult(tf_FOnResult &&_fOnResult, NMeta::TCIndices<tfp_Indices...> _Indices) &&;
	};

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCBoundActorCallAwaiter
	{
		TCBoundActorCallAwaiter(t_CBoundFunctor &&_BoundFunctor, t_FExceptionTransform &&_fExceptionTransform);

		~TCBoundActorCallAwaiter();

		bool await_ready() noexcept;
		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle) noexcept(NTraits::TCIsSame<t_FExceptionTransform, CVoidTag>::mc_Value);
		auto await_resume() noexcept;

	private:
		t_CBoundFunctor &&mp_BoundFunctor;
		TCAsyncResult<t_CReturn> mp_AsyncResult;
		DMibNoUniqueAddress t_FExceptionTransform mp_fExceptionTransform;
	};
}

#include "Malterlib_Concurrency_BoundActorCall.hpp"
