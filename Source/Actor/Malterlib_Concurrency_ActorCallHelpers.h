// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/RunLoop>
#include <Mib/Storage/Optional>
#include <Mib/Time/Stopwatch>

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename t_CTypeList>
		struct TCActorCallWithParamsTuple
		{
		};

		template <typename ...tp_CParam>
		struct TCActorCallWithParamsTuple<NMeta::TCTypeList<tp_CParam...>>
		{
			using CType = NStorage::TCTuple<NTraits::TCRemoveReferenceAndQualifiers<tp_CParam>...>;
		};

		template <typename t_FMemberFunctionPointer, bool t_bIsMemberFunctionPointer = NTraits::cIsMemberFunctionPointer<t_FMemberFunctionPointer>>
		struct TCActorCallWithParams
		{
			using CMemberPointerTypes = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CParams;

			static constexpr umint mc_nParams = NMeta::gc_TypeList_Len<CMemberPointerTypes>;
			using CReturnType = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CReturn;
			using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CClass;
			using CIndices = NMeta::TCConsecutiveIndices<mc_nParams>;

			template <typename ...tfp_CParams>
			TCActorCallWithParams(t_FMemberFunctionPointer _fMemberPointer, tfp_CParams && ...p_Params);

			TCActorCallWithParams(TCActorCallWithParams &&_Other);

			template <typename tf_CActor, umint ...tfp_Indices>
			mark_artificial inline_always CReturnType operator () (tf_CActor &_Actor, NMeta::TCIndices<tfp_Indices...> _Indices);

			t_FMemberFunctionPointer m_fMemberPointer;
			typename TCActorCallWithParamsTuple<CMemberPointerTypes>::CType m_Params;

		private:
			TCActorCallWithParams(TCActorCallWithParams const &_Other);
			TCActorCallWithParams &operator = (TCActorCallWithParams const &_Other);
			TCActorCallWithParams &operator = (TCActorCallWithParams &&_Other);
		};

		template <typename t_FFunctionType>
		struct TCActorCallWithParams<t_FFunctionType, false>
		{
			using CTraits = NTraits::TCFunctionTraits<t_FFunctionType>;
			using CMemberPointerTypes = typename CTraits::CParams;

			static constexpr umint mc_nParams = NMeta::gc_TypeList_Len<CMemberPointerTypes>;
			using CReturnType = typename CTraits::CReturn;
			using CIndices = NMeta::TCConsecutiveIndices<mc_nParams>;

			template <typename ...tfp_CParams>
			TCActorCallWithParams(t_FFunctionType *_pFunctionPointer, tfp_CParams && ...p_Params);

			TCActorCallWithParams(TCActorCallWithParams &&_Other);

			template <typename tf_CActor, umint ...tfp_Indices>
			mark_artificial inline_always CReturnType operator () (tf_CActor &_Actor, NMeta::TCIndices<tfp_Indices...> _Indices);

			t_FFunctionType *m_pFunctionPointer;
			typename TCActorCallWithParamsTuple<CMemberPointerTypes>::CType m_Params;

		private:
			TCActorCallWithParams(TCActorCallWithParams const &_Other);
			TCActorCallWithParams &operator = (TCActorCallWithParams const &_Other);
			TCActorCallWithParams &operator = (TCActorCallWithParams &&_Other);
		};

		template <typename ...tfp_CParams>
		auto fg_ToTupleByValue(NStorage::TCTuple<tfp_CParams...> &&_Params);

		template <typename ...tfp_CParams, umint ...tfp_Indicies>
		auto fg_ToTupleByValueHelper(TCConstruct<void, tfp_CParams...> &&_Params, NMeta::TCIndices<tfp_Indicies...> _Indices);

		template <typename ...tfp_CParams>
		auto fg_ToTupleByValue(TCConstruct<void, tfp_CParams...> &&_Params);
	}

	template <typename t_CActor, typename t_CFunctor>
	struct TCActorResultCall
	{
		static_assert(!NTraits::cIsReference<t_CActor>, "Incorrect type");
		static_assert(!NTraits::cIsReference<t_CFunctor>, "Incorrect type");

		t_CActor m_Actor;
		t_CFunctor m_Functor;

		TCActorResultCall(t_CActor const &_Actor, t_CFunctor &&_Functor);
		TCActorResultCall(TCActorResultCall const &_Other);
		TCActorResultCall(TCActorResultCall &&_Other);

		template <typename ...tfp_CParams>
		void operator () (tfp_CParams && ...p_Params);

		TCActorResultCall &operator = (TCActorResultCall const &_Other);
		TCActorResultCall &operator = (TCActorResultCall &&_Other);
	};

	namespace NPrivate
	{
		struct CDummyRunLoop
		{
			void f_Wait();
			bool f_WaitTimeout(fp64 _Timeout);
			void f_Finished();

			NThread::CEvent m_WaitEvent;
		};

		struct CRunLoopState
		{
			CRunLoopState(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop);

			void f_Wait();
			bool f_WaitTimeout(fp64 _Timeout);
			void f_Finished();

			NAtomic::TCAtomic<bool> m_bFinished = false;
			NStorage::TCSharedPointer<CRunLoop> m_pManualRunLoop;
		};

		template <typename t_CReturnType, typename t_CRunLoop = CDummyRunLoop>
		struct TCCallSyncState
		{
			template <typename ...tfp_CParams>
			TCCallSyncState(tfp_CParams && ...p_Params);

			~TCCallSyncState();

			template <typename tf_CResult>
			void f_SetResult(tf_CResult &&_Result);

			template <umint ...tfp_Indices, typename ...tfp_CResults>
			void f_TransferResults(NMeta::TCIndices<tfp_Indices...> const &_Indicies, tfp_CResults && ...p_Results);

			void f_SetException(NException::CExceptionPointer &&_pException);

			t_CRunLoop m_RunLoop;
			NThread::CMutual m_Lock;
			TCAsyncResult<t_CReturnType> m_Result;
		};

		template <typename t_CType>
		struct TCConvertResultTypeVoid
		{
			using CType = t_CType;
		};

		template <>
		struct TCConvertResultTypeVoid<void>
		{
			using CType = CVoidTag;
		};
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename ...tp_CFutures>
	struct TCFuturePackAwaiter
	{
		using CUnwrappedType = NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CFutures::CValue>::CType...>;
		using CWrappedType = NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>;

		TCFuturePackAwaiter(NStorage::TCTuple<tp_CFutures...> &&_Futures, t_FExceptionTransform &&_fExceptionTransform);
		~TCFuturePackAwaiter();

		bool await_ready() const noexcept;
		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle);
		auto await_resume() noexcept(!t_bUnwrap);

	private:
		template <typename tf_CResultFunctor, umint ...tfp_Indices>
		void fp_ActorCall(tf_CResultFunctor &&_ResultFunctor, TCActor<> &&_ResultActor, NMeta::TCIndices<tfp_Indices...> _Indices);

		template <umint ...tfp_Indices>
		CUnwrappedType fp_Unwrap(NMeta::TCIndices<tfp_Indices...> _Indices);

		template <umint ...tfp_Indices>
		NException::CExceptionPointer fp_HasException(NMeta::TCIndices<tfp_Indices...> _Indices);

		template <umint ...tfp_Indices>
		void fp_TransformExceptions(CWrappedType &o_Results, NMeta::TCIndices<tfp_Indices...> _Indices);

		CWrappedType mp_Results;
		NStorage::TCTuple<tp_CFutures...> mp_Futures;
		NAtomic::TCAtomic<umint> mp_ResultAvailable;
		DMibNoUniqueAddress t_FExceptionTransform mp_fExceptionTransform;
	};

	template <typename t_CFuturePack, typename ...tp_CFutures>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFuturePackWithError : protected CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCFuturePackWithError *m_pWrapped;
			auto operator co_await () &&;
		};

		TCFuturePackWithError(t_CFuturePack *_pFuturePack, FExceptionTransformer &&_fTransfomer);

		auto operator co_await () &&;
		CNoUnwrapAsyncResult f_Wrap() &&;

	private:
		t_CFuturePack *mp_pFuturePack;
	};

	template <typename ...tp_CFutures>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFuturePack
	{
		NStorage::TCTuple<tp_CFutures...> m_Futures;

		TCFuturePack(NStorage::TCTuple<tp_CFutures...> &&_Tuple);

		template <typename tf_CReturn, typename tf_CFutureLike>
		auto operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Future) &&;

		template <typename tf_CType>
		TCFuturePack<tp_CFutures..., TCFuture<tf_CType>> operator + (TCFuture<tf_CType> &&_Future) &&;

		template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
		auto operator + (TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other) &&
			-> TCFuturePack<tp_CFutures..., TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
		;

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&;

		template <typename tf_CFunctor>
		void operator > (tf_CFunctor &&_Functor) &&
			requires (!cIsActorResultCall<tf_CFunctor>)
		;

		auto f_CallSync(fp64 _Timeout = -1.0) &&;
		auto f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout = -1.0) &&;

		struct CNoUnwrapAsyncResult
		{
			TCFuturePack *m_pWrapped;
			auto operator co_await () &&;
		};

		CNoUnwrapAsyncResult f_Wrap() &&;

		auto operator co_await () &&;

		auto operator % (NStr::CStr const &_ErrorString) && -> TCFuturePackWithError<TCFuturePack, tp_CFutures...>;
		auto operator ^ (NStr::CStr const &_ErrorString) && -> TCFuturePackWithError<TCFuturePack, tp_CFutures...>;

	private:
		template <typename tf_CRunLoop, typename ...tfp_CParams, umint ...tfp_Indices>
		auto fp_CallSync(fp64 _Timeout, NMeta::TCIndices<tfp_Indices...> _Indices, tfp_CParams && ...p_Params) &&
			-> NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CFutures::CValue>::CType...>
		;

		template <typename tf_CResultActor, typename tf_CResultFunctor, umint ...tfp_Indices>
		void fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...> _Indices) &&
			requires (NTraits::cIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<typename tp_CFutures::CValue> && ...p_Results)>) // Incorrect types in result call
		;

		template <umint ...tfp_Indices>
		TCFuture<NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>> fp_ActorFutureCall(NMeta::TCIndices<tfp_Indices...> _Indices) &&;
	};

	namespace NPrivate
	{
		template <typename t_CType, typename t_CStrippedType = typename NTraits::TCMemberFunctionPointerTraits<t_CType>::CReturn>
		struct TCGetResultType
		{
			using CType = t_CStrippedType;
		};

		template <typename t_CType, typename t_CReturnType>
		struct TCGetResultType<t_CType, TCPromise<t_CReturnType>>
		{
			using CType = t_CReturnType;
		};

		template <typename t_CType, typename t_CReturnType>
		struct TCGetResultType<t_CType, TCFuture<t_CReturnType>>
		{
			using CType = t_CReturnType;
		};

		template <bool t_bUnwrapTuple, typename t_CHandler, typename t_CActor, typename ...tp_CResultTypes, umint ...tp_ResultIndices>
		struct TCCallMutipleActorStorage<t_bUnwrapTuple, t_CHandler, t_CActor, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>>
		{
			static constexpr aint mc_nResults = sizeof...(tp_CResultTypes);

			NStorage::CIntrusiveRefCount m_RefCount;
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<umint> m_nFinished;
			t_CActor m_Actor;
			t_CHandler m_Handler;

			TCCallMutipleActorStorage(t_CActor &&_Actor, t_CHandler &&_Handler);
			~TCCallMutipleActorStorage();

			void f_ReportResult();
			void f_Finished();
		};

		template <typename t_CResultTypes, typename t_CResultIndicies = NMeta::TCConsecutiveIndices<NMeta::gc_TypeList_Len<t_CResultTypes>>>
		struct TCCallMutipleFutureStorage;

		template <typename ...tp_CResultTypes, umint ...tp_ResultIndices>
		struct TCCallMutipleFutureStorage<NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>>
		{
			static constexpr aint mc_nResults = sizeof...(tp_CResultTypes);

			NStorage::CIntrusiveRefCount m_RefCount;
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<umint> m_nFinished;
			TCPromiseFuturePair<NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...>> m_Promise;

			TCCallMutipleFutureStorage() = default;

			~TCCallMutipleFutureStorage();

			void f_ReportResult();
			void f_Finished();
		};
	}

	struct CReportLocalState
	{
		CReportLocalState(CReportLocalState &&_Other) = default;
		CReportLocalState();
		~CReportLocalState();
		void f_StoreCallStates(CSystemThreadLocal &_ThreadLocal);
		NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> f_RestoreCallStates();
#if DMibConfig_Concurrency_DebugActorCallstacks
		void f_CaptureCallstack();
#endif

		NContainer::TCVector<NFunction::TCFunctionMovable<void ()>> m_RestoreStates;
#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks m_Callstacks;
#endif
	};

	template <typename t_CActor, typename t_CRet, typename t_CFunctor>
	struct TCReportLocalFuture final : public CConcurrentRunQueueEntry
	{
		static constexpr bool const mc_bIsFuture = NPrivate::TCIsFuture<typename t_CFunctor::CReturnType>::mc_Value;
		static constexpr bool const mc_bIsAsyncGenerator = NPrivate::TCIsAsyncGenerator<typename t_CFunctor::CReturnType>::mc_Value;

		TCReportLocalFuture(TCReportLocalFuture const &_Other) = delete;

		TCReportLocalFuture(TCReportLocalFuture &&_Other);
		TCReportLocalFuture(t_CFunctor &&_ToCall, TCActorInternal<t_CActor> *_pActorInternal, TCPromise<t_CRet> &&_Promise, CSystemThreadLocal &_ThreadLocal, bool _bStoreCallStates);

		void f_Cleanup() override;
		void f_Call(CConcurrencyThreadLocal &_ThreadLocal) override;
		void f_CallNoDelete(CConcurrencyThreadLocal &_ThreadLocal);
		inline_always void f_CallShared(CConcurrencyThreadLocal &_ThreadLocal);

		~TCReportLocalFuture();

		CReportLocalState m_State;
		t_CFunctor m_ToCall;
		TCPromise<t_CRet> m_Promise;
		TCActorInternal<t_CActor> *m_pActorInternal;
#if DMibConfig_Concurrency_DebugBlockDestroy
		NStr::CStr m_ActorTypeName;
#endif
	};

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_FFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_FFunctor const &_fMemberFunctor
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...> _FunctionParamTypes
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType<typename NTraits::TCMemberFunctionPointerTraits<tf_FFunctor>::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
		requires
		(
			NTraits::cIsMemberFunctionPointer<tf_FFunctor>
			&& !
			(
				!NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
				&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
			)
		)
	;

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_FFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_FFunctor const &_fMemberFunctor
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...> _FunctionParamTypes
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType<typename NTraits::TCMemberFunctionPointerTraits<tf_FFunctor>::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
		requires
		(
			NTraits::cIsMemberFunctionPointer<tf_FFunctor>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
			&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
		)
	;

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_FFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_FFunctor const &_fFunctionPointer
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...> _FunctionParamTypes
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType<typename NTraits::TCFunctionTraits<NTraits::TCRemovePointer<tf_FFunctor>>::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
		requires
		(
			!NTraits::cIsMemberFunctionPointer<tf_FFunctor>
			&& !
			(
				!NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
				&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
			)
		)
	;

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_FFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_FFunctor const &_fFunctionPointer
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...> _FunctionParamTypes
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType<typename NTraits::TCFunctionTraits<NTraits::TCRemovePointer<tf_FFunctor>>::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
		requires
		(
			!NTraits::cIsMemberFunctionPointer<tf_FFunctor>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
			&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
		)
	;

	namespace NPrivate
	{
		template <typename tf_CParam>
		auto fg_ConvertToFuture(tf_CParam &&_Param)
			requires (NPrivate::TCIsFuture<NTraits::TCRemoveReferenceAndQualifiers<tf_CParam>>::mc_Value)
		;

		template <typename tf_CParam>
		auto fg_ConvertToFuture(tf_CParam &&_Param);

		template <typename ...tfp_CParams, umint ...tfp_Indidies>
		auto fg_AnyDoneImpl(NMeta::TCIndices<tfp_Indidies...> const &_Indices, TCFuture<tfp_CParams> && ...p_Futures)
			-> TCFuture<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>>
		;

		template <typename ...tfp_CParams, umint ...tfp_Indidies>
		auto fg_AnySuccessImpl(NMeta::TCIndices<tfp_Indidies...> const &_Indices, TCFuture<tfp_CParams> && ...p_Futures)
			-> TCFuture<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>>
		;
	}

	template <typename ...tfp_CParams>
	auto fg_AnyDone(tfp_CParams && ...p_Params);

	template <typename ...tfp_CParams>
	auto fg_AnySuccess(tfp_CParams && ...p_Params);
}

#include "Malterlib_Concurrency_ActorCallHelpers.hpp"
#include "Malterlib_Concurrency_ActorCallCoroutine.hpp"
