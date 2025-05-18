// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/Actor/Timer>
#include <Mib/Concurrency/RunLoop>
#include <Mib/Storage/Optional>

namespace NMib::NConcurrency
{
	CCurrentActorScope::CCurrentActorScope(CConcurrencyThreadLocal &_ThreadLocal, TCActor<CActor> const &_Actor)
		: mp_ThreadLocal(_ThreadLocal)
	{
		mp_pLastActor = _ThreadLocal.m_pCurrentlyProcessingActorHolder;
		_ThreadLocal.m_pCurrentlyProcessingActorHolder = _Actor.f_Get();

		mp_bLastProcessing = _ThreadLocal.m_bCurrentlyProcessingInActorHolder;
		_ThreadLocal.m_bCurrentlyProcessingInActorHolder = false;
	}

	CCurrentActorScope::CCurrentActorScope(CConcurrencyThreadLocal &_ThreadLocal, CActorHolder *_pActor)
		: mp_ThreadLocal(_ThreadLocal)
	{
		mp_pLastActor = _ThreadLocal.m_pCurrentlyProcessingActorHolder;
		_ThreadLocal.m_pCurrentlyProcessingActorHolder = _pActor;

		mp_bLastProcessing = _ThreadLocal.m_bCurrentlyProcessingInActorHolder;
		_ThreadLocal.m_bCurrentlyProcessingInActorHolder = false;
	}

	CCurrentActorScope::~CCurrentActorScope()
	{
		mp_ThreadLocal.m_pCurrentlyProcessingActorHolder = mp_pLastActor;
		mp_ThreadLocal.m_bCurrentlyProcessingInActorHolder = mp_bLastProcessing;
	}

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

			static constexpr mint mc_nParams = NMeta::gc_TypeList_Len<CMemberPointerTypes>;
			using CReturnType = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CReturn;
			using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CClass;
			using CIndices = NMeta::TCConsecutiveIndices<mc_nParams>;

			template <typename... tfp_CParams>
			TCActorCallWithParams(t_FMemberFunctionPointer _pMemberPointer, tfp_CParams &&...p_Params)
				: m_pMemberPointer(_pMemberPointer)
				, m_Params(fg_Forward<tfp_CParams>(p_Params)...)
			{
			}

			TCActorCallWithParams(TCActorCallWithParams &&_Other)
				: m_pMemberPointer(fg_Move(_Other.m_pMemberPointer))
				, m_Params(fg_Move(_Other.m_Params))
			{
			}

			template <typename tf_CActor, mint ...tfp_Indices>
			mark_artificial inline_always CReturnType operator() (tf_CActor &_Actor, NMeta::TCIndices<tfp_Indices...>)
			{
				return (_Actor.*m_pMemberPointer)(fg_Move(fg_Get<tfp_Indices>(m_Params))...);
			}

			t_FMemberFunctionPointer m_pMemberPointer;
			typename TCActorCallWithParamsTuple<CMemberPointerTypes>::CType m_Params;

		private:
			TCActorCallWithParams(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams &&);
		};

		template <typename t_FFunctionType>
		struct TCActorCallWithParams<t_FFunctionType, false>
		{
			using CTraits = NTraits::TCFunctionTraits<t_FFunctionType>;
			using CMemberPointerTypes = typename CTraits::CParams;

			static constexpr mint mc_nParams = NMeta::gc_TypeList_Len<CMemberPointerTypes>;
			using CReturnType = typename CTraits::CReturn;
			using CIndices = NMeta::TCConsecutiveIndices<mc_nParams>;

			template <typename... tfp_CParams>
			TCActorCallWithParams(t_FFunctionType *_pFunctionPointer, tfp_CParams &&...p_Params)
				: m_pFunctionPointer(_pFunctionPointer)
				, m_Params(fg_Forward<tfp_CParams>(p_Params)...)
			{
			}

			TCActorCallWithParams(TCActorCallWithParams &&_Other)
				: m_pFunctionPointer(fg_Move(_Other.m_pFunctionPointer))
				, m_Params(fg_Move(_Other.m_Params))
			{
			}

			template <typename tf_CActor, mint ...tfp_Indices>
			mark_artificial inline_always CReturnType operator() (tf_CActor &_Actor, NMeta::TCIndices<tfp_Indices...>)
			{
				return m_pFunctionPointer(fg_Move(fg_Get<tfp_Indices>(m_Params))...);
			}

			t_FFunctionType *m_pFunctionPointer;
			typename TCActorCallWithParamsTuple<CMemberPointerTypes>::CType m_Params;

		private:
			TCActorCallWithParams(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams &&);
		};

		template <typename... tfp_CParams>
		auto fg_ToTupleByValue(NStorage::TCTuple<tfp_CParams...> &&_Params)
		{
			return fg_Move(_Params);
		}

		template <typename... tfp_CParams, mint... tfp_Indicies>
		auto fg_ToTupleByValueHelper(TCConstruct<void, tfp_CParams...> &&_Params, NMeta::TCIndices<tfp_Indicies...>)
		{
			return NStorage::fg_Tuple
				(
					fg_Get<tfp_Indicies>(fg_Move(_Params.m_Params))...
				)
			;
		}

		template <typename... tfp_CParams>
		auto fg_ToTupleByValue(TCConstruct<void, tfp_CParams...> &&_Params)
		{
			return fg_ToTupleByValueHelper(fg_Move(_Params), NMeta::TCConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>());
		}


	}

	template <typename t_CActor, typename t_CFunctor>
	struct TCActorResultCall
	{
		static_assert(!NTraits::cIsReference<t_CActor>, "Incorrect type");
		static_assert(!NTraits::cIsReference<t_CFunctor>, "Incorrect type");

		t_CActor mp_Actor;
		t_CFunctor mp_Functor;

		TCActorResultCall(t_CActor const &_Actor, t_CFunctor &&_Functor)
			: mp_Actor(_Actor)
			, mp_Functor(fg_Move(_Functor))
		{
		}
		TCActorResultCall(TCActorResultCall const &_Other)
			: mp_Actor(_Other.mp_Actor)
			, mp_Functor(_Other.mp_Functor)
		{
		}

		TCActorResultCall(TCActorResultCall &&_Other)
			: mp_Actor(fg_Move(_Other.mp_Actor))
			, mp_Functor(fg_Move(_Other.mp_Functor))
		{
		}


		template <typename ...tfp_CParams>
		void operator () (tfp_CParams && ...p_Params)
		{
			DMibFastCheck(mp_Actor);

			auto pActor = mp_Actor.f_GetRealActor();
			pActor->f_QueueProcess
				(
					[
						...Params = fg_Forward<tfp_CParams>(p_Params), Functor = fg_Move(mp_Functor)
#if DMibEnableSafeCheck > 0
						,pActor
#endif
					]
					(CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == pActor);
						Functor(fg_Move(Params)...);
					}
				)
			;
		}

		TCActorResultCall &operator =(TCActorResultCall const &_Other)
		{
			mp_Actor = _Other.mp_Actor;
			mp_Functor = _Other.mp_Functor;
		}

		TCActorResultCall &operator =(TCActorResultCall &&_Other)
		{
			mp_Actor = fg_Move(_Other.mp_Actor);
			mp_Functor = fg_Move(_Other.mp_Functor);
		}

	};

	namespace NPrivate
	{
		struct CDummyRunLoop
		{
			void f_Wait()
			{
				m_WaitEvent.f_Wait();
			}

			bool f_WaitTimeout(fp64 _Timeout)
			{
				return m_WaitEvent.f_WaitTimeout(_Timeout);
			}

			void f_Finished()
			{
				m_WaitEvent.f_SetSignaled();
			}

			NThread::CEvent m_WaitEvent;
		};

		struct CRunLoopState
		{
			CRunLoopState(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop)
				: m_pManualRunLoop(_pRunLoop)
			{
			}

			void f_Wait()
			{
				while (!m_bFinished.f_Load())
					m_pManualRunLoop->f_WaitOnce();
			}

			bool f_WaitTimeout(fp64 _Timeout)
			{
				NTime::CClock Clock(true);
				while (!m_bFinished.f_Load())
				{
					fp64 ToWait = _Timeout - Clock.f_GetTime();
					if (ToWait < 0.0)
						return true;

					if (m_pManualRunLoop->f_WaitOnceTimeout(ToWait))
						return true;
				}

				return false;
			}

			void f_Finished()
			{
				m_bFinished = true;
				m_pManualRunLoop->f_Wake();
			}

			NAtomic::TCAtomic<bool> m_bFinished = false;
			NStorage::TCSharedPointer<CRunLoop> m_pManualRunLoop;
		};

		template <typename t_CReturnType, typename t_CRunLoop = CDummyRunLoop>
		struct TCCallSyncState
		{
			template <typename ...tfp_Params>
			TCCallSyncState(tfp_Params && ...p_Params)
				: m_RunLoop(fg_Forward<tfp_Params>(p_Params)...)
			{
			}

			~TCCallSyncState()
			{
				DMibLock(m_Lock);
				m_Result.f_Clear();
			}

			template <typename tf_CResult>
			void f_SetResult(tf_CResult &&_Result)
			{
				{
					DMibLock(m_Lock);
					m_Result = fg_Forward<tf_CResult>(_Result);
				}

				m_RunLoop.f_Finished();
			}

			template <mint ...tfp_Indices, typename ...tfp_CResults>
			void f_TransferResults(NMeta::TCIndices<tfp_Indices...> const &_Indicies, tfp_CResults && ...p_Results)
			{
				t_CReturnType Result;

				bool bResultSet = false;

				(
					[&]
					{
						if (!p_Results)
						{
							DMibLock(m_Lock);
							bResultSet = true;
							m_Result.f_SetExceptionAppendable(fg_Move(p_Results).f_GetException());
						}
						else
							fg_Get<tfp_Indices>(Result) = fg_Move(*p_Results);
					}
					()
					, ...
				);

				if (!bResultSet)
				{
					DMibLock(m_Lock);
					m_Result.f_SetResult(fg_Move(Result));
				}

				m_RunLoop.f_Finished();
			}

			void f_SetException(NException::CExceptionPointer &&_pException)
			{
				{
					DMibLock(m_Lock);
					m_Result.f_SetException(fg_Move(_pException));
				}

				m_RunLoop.f_Finished();
			}

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

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CFutures>
	struct TCFuturePackAwaiter
	{
		using CUnwrappedType = NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CFutures::CValue>::CType...>;
		using CWrappedType = NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>;

		TCFuturePackAwaiter(NStorage::TCTuple<tp_CFutures...> &&_Futures, t_FExceptionTransform &&_fExceptionTransform);
		~TCFuturePackAwaiter();

		template
		<
			typename tf_CResultFunctor
			, mint... tfp_Indices
		>
		void fp_ActorCall(tf_CResultFunctor &&_ResultFunctor, TCActor<> &&_ResultActor, NMeta::TCIndices<tfp_Indices...>);

		template <mint... tfp_Indices>
		CUnwrappedType fp_Unwrap(NMeta::TCIndices<tfp_Indices...>);

		template <mint... tfp_Indices>
		NException::CExceptionPointer fp_HasException(NMeta::TCIndices<tfp_Indices...>);

		template <mint... tfp_Indices>
		void fp_TransformExceptions(CWrappedType &o_Results, NMeta::TCIndices<tfp_Indices...>);

		bool await_ready() const noexcept;
		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle);
		auto await_resume() noexcept(!t_bUnwrap);

	private:
		CWrappedType mp_Results;
		NStorage::TCTuple<tp_CFutures...> mp_Futures;
		NAtomic::TCAtomic<mint> mp_ResultAvailable;
		DMibNoUniqueAddress t_FExceptionTransform mp_fExceptionTransform;
	};

	template <typename t_CFuturePack, typename... tp_CFutures>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFuturePackWithError : protected CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCFuturePackWithError *m_pWrapped;
			auto operator co_await() &&;
		};

		TCFuturePackWithError(t_CFuturePack *_pFuturePack, FExceptionTransformer &&_fTransfomer);

		auto operator co_await() &&;
		CNoUnwrapAsyncResult f_Wrap() &&;

	private:

		t_CFuturePack *mp_pFuturePack;
	};

	template <typename... tp_CFutures>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFuturePack
	{
		NStorage::TCTuple<tp_CFutures...> m_Futures;

		TCFuturePack(NStorage::TCTuple<tp_CFutures...> &&_Tuple)
			: m_Futures(fg_Move(_Tuple))
		{
		}

		template <typename tf_CReturn, typename tf_CFutureLike>
		auto operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Future) &&
		{
			return fg_Move(*this) + fg_Move(_Future).f_Dispatch();
		}

		template <typename tf_CType>
		TCFuturePack<tp_CFutures..., TCFuture<tf_CType>> operator + (TCFuture<tf_CType> &&_Future) &&;

		template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
		auto operator + (TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other) &&
			-> TCFuturePack<tp_CFutures..., TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
		;

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
		{
			fg_Move(*this).fp_ActorCall(fg_Move(_ResultCall), NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());
		}

		template <typename tf_CFunctor>
		void operator > (tf_CFunctor &&_Functor) &&
			requires (!cIsActorResultCall<tf_CFunctor>)
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			fg_Move(*this).fp_ActorCall(fg_Move(pActor) / fg_Forward<tf_CFunctor>(_Functor), NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());
		}

		auto f_CallSync(fp64 _Timeout = -1.0) &&
		{
			return fg_Move(*this).template fp_CallSync<NPrivate::CDummyRunLoop>(_Timeout, NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());
		}

		auto f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout = -1.0) &&
		{
			return fg_Move(*this).template fp_CallSync<NPrivate::CRunLoopState>(_Timeout, NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>(), _pRunLoop);
		}

		struct CNoUnwrapAsyncResult
		{
			TCFuturePack *m_pWrapped;
			auto operator co_await() &&
			{
				return TCFuturePackAwaiter<false, CVoidTag, tp_CFutures...>{fg_Move(m_pWrapped->m_Futures), {}};
			}
		};

		CNoUnwrapAsyncResult f_Wrap() &&
		{
			return {this};
		}

		auto operator co_await() &&
		{
			return TCFuturePackAwaiter<true, CVoidTag, tp_CFutures...>{fg_Move(m_Futures), {}};
		}

		auto operator % (NStr::CStr const &_ErrorString) && -> TCFuturePackWithError<TCFuturePack, tp_CFutures...>
		{
			return {this, fg_ExceptionTransformer(nullptr, _ErrorString)};
		}

		auto operator ^ (NStr::CStr const &_ErrorString) && -> TCFuturePackWithError<TCFuturePack, tp_CFutures...>
		{
			return {this, fg_ExceptionTransformer(nullptr, _ErrorString)};
		}

	private:
		template <typename tf_CRunLoop, typename ...tfp_CParams, mint... tfp_Indices>
		NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CFutures::CValue>::CType...> fp_CallSync
			(
				fp64 _Timeout
				, NMeta::TCIndices<tfp_Indices...>
				, tfp_CParams && ...p_Params
			) &&
		{
			auto Future = fg_Move(*this).fp_ActorFutureCall(NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>());

			using CReturnType = NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CFutures::CValue>::CType...>;
			NStorage::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType, tf_CRunLoop>> pResult = fg_Construct(fg_Forward<tfp_CParams>(p_Params)...);

			Future.f_OnResultSet
				(
					[pResult](TCAsyncResult<NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>> &&_Results)
					{
						if (!_Results)
						{
							pResult->f_SetException(_Results.f_GetException());
							return;
						}
						auto &Results = *_Results;
						pResult->f_TransferResults(NMeta::TCConsecutiveIndices<sizeof...(tp_CFutures)>(), fg_Get<tfp_Indices>(Results)...);
					}
				)
			;

			if (_Timeout >= 0.0)
			{
				if (pResult->m_RunLoop.f_WaitTimeout(_Timeout))
					DMibError("Timed out waiting for futures to resolve synchronously");
			}
			else
				pResult->m_RunLoop.f_Wait();

			DMibLock(pResult->m_Lock);
			return pResult->m_Result.f_Move();
		}

		template <typename tf_CResultActor, typename tf_CResultFunctor, mint... tfp_Indices>
		void fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>) &&
			requires (NTraits::cIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<typename tp_CFutures::CValue> && ...)>) // Incorrect types in result call
		;

		template <mint... tfp_Indices>
		TCFuture<NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>> fp_ActorFutureCall(NMeta::TCIndices<tfp_Indices...>) &&;
	};

	namespace NPrivate
	{
		template <typename t_CType, typename t_CStrippedType = typename NTraits::TCMemberFunctionPointerTraits<t_CType>::CReturn>
		struct TCGetResultType
		{
			typedef t_CStrippedType CType;
		};

		template <typename t_CType, typename t_CReturnType>
		struct TCGetResultType<t_CType, TCPromise<t_CReturnType>>
		{
			typedef t_CReturnType CType;
		};

		template <typename t_CType, typename t_CReturnType>
		struct TCGetResultType<t_CType, TCFuture<t_CReturnType>>
		{
			typedef t_CReturnType CType;
		};

		template
		<
			bool t_bUnwrapTuple
			, typename t_CHandler
			, typename t_CActor
			, typename... tp_CResultTypes
			, mint... tp_ResultIndices
		>
		struct TCCallMutipleActorStorage<t_bUnwrapTuple, t_CHandler, t_CActor, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};
			NStorage::CIntrusiveRefCount m_RefCount;
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<mint> m_nFinished;
			t_CActor m_Actor;
			t_CHandler m_Handler;

			TCCallMutipleActorStorage(t_CActor &&_Actor, t_CHandler &&_Handler)
				: m_Handler(fg_Move(_Handler))
				, m_Actor(fg_Move(_Actor))
			{
			}

			~TCCallMutipleActorStorage()
			{
				if (m_nFinished.f_Load() != mc_nResults)
					f_ReportResult();
			}

			void f_ReportResult()
			{
				NStorage::TCSharedPointer<TCCallMutipleActorStorage> pThis = fg_Explicit(this);
				m_Actor.f_GetRealActor()->f_QueueProcess
					(
						[pThis, Handler = fg_Move(m_Handler)](CConcurrencyThreadLocal &_ThreadLocal) mutable
						{
							auto &This = *pThis;
							DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == This.m_Actor.f_GetRealActor());

							if constexpr (t_bUnwrapTuple)
								Handler(fg_Move(fg_Get<tp_ResultIndices>(This.m_Results))...);
							else
								Handler(fg_Move(This.m_Results));
						}
					)
				;
			}
			void f_Finished()
			{
				if (++m_nFinished == mc_nResults)
					f_ReportResult();
			}
		};

		template
		<
			typename t_CResultTypes
			, typename t_CResultIndicies = NMeta::TCConsecutiveIndices<NMeta::gc_TypeList_Len<t_CResultTypes>>
		>
		struct TCCallMutipleFutureStorage;

		template
		<
			typename... tp_CResultTypes
			, mint... tp_ResultIndices
		>
		struct TCCallMutipleFutureStorage<NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};

			NStorage::CIntrusiveRefCount m_RefCount;
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<mint> m_nFinished;
			TCPromiseFuturePair<NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...>> m_Promise;

			TCCallMutipleFutureStorage() = default;

			~TCCallMutipleFutureStorage()
			{
				if (m_nFinished.f_Load() != mc_nResults)
					f_ReportResult();
			}

			void f_ReportResult()
			{
				m_Promise.m_Promise.f_SetResult(fg_Move(m_Results));
				m_Promise.m_Promise.f_ClearResultSet();
			}

			void f_Finished()
			{
				if (++m_nFinished == mc_nResults)
					f_ReportResult();
			}
		};
	}

	template <typename... tp_CFutures>
	template
	<
		typename tf_CResultActor
		, typename tf_CResultFunctor
		, mint... tfp_Indices
	>
	void TCFuturePack<tp_CFutures...>::fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>) &&
		requires (NTraits::cIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<typename tp_CFutures::CValue> && ...)>) // Incorrect types in result call
	{
		using CTypeList = NMeta::TCTypeList<typename tp_CFutures::CValue...>;
		using CStorage = NPrivate::TCCallMutipleActorStorage<true, tf_CResultFunctor, tf_CResultActor, CTypeList>;

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(fg_Move(_ResultCall.mp_Actor), fg_Move(_ResultCall.mp_Functor));

		(
			[&]
			{
				fg_Move(fg_Get<tfp_Indices>(m_Futures)).f_OnResultSet
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

	template <typename... tp_CFutures>
	template <mint... tfp_Indices>
	TCFuture<NStorage::TCTuple<TCAsyncResult<typename tp_CFutures::CValue>...>> TCFuturePack<tp_CFutures...>::fp_ActorFutureCall(NMeta::TCIndices<tfp_Indices...>) &&
	{
		using CTypeList = NMeta::TCTypeList<typename tp_CFutures::CValue...>;
		using CStorage = NPrivate::TCCallMutipleFutureStorage<CTypeList>;

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct();

		(
			[&]
			{
				fg_Move(fg_Get<tfp_Indices>(m_Futures)).f_OnResultSet
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

		return fg_Move(pStorage->m_Promise.m_Future);
	}

	struct CReportLocalState
	{
		CReportLocalState(CReportLocalState &&) = default;
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

	template
	<
		typename t_CActor
		, typename t_CRet
		, typename t_CFunctor
	>
	struct TCReportLocalFuture final : public CConcurrentRunQueueEntry
	{
		constexpr static const bool mc_bIsFuture = NPrivate::TCIsFuture<typename t_CFunctor::CReturnType>::mc_Value;
		constexpr static const bool mc_bIsAsyncGenerator = NPrivate::TCIsAsyncGenerator<typename t_CFunctor::CReturnType>::mc_Value;

		TCReportLocalFuture(TCReportLocalFuture const &_Other) = delete;

		TCReportLocalFuture(TCReportLocalFuture &&_Other)
			: m_State(fg_Move(_Other.m_State))
			, m_ToCall(fg_Move(_Other.m_ToCall))
			, m_Promise(fg_Move(_Other.m_Promise))
			, m_pActorInternal(fg_Exchange(_Other.m_pActorInternal, nullptr))
#if DMibConfig_Concurrency_DebugBlockDestroy
			, m_ActorTypeName(fg_Move(_Other.m_ActorTypeName))
#endif
		{
		}

		TCReportLocalFuture
			(
				t_CFunctor &&_ToCall
				, TCActorInternal<t_CActor> *_pActorInternal
				, TCPromise<t_CRet> &&_Promise
				, CSystemThreadLocal &_ThreadLocal
				, bool _bStoreCallStates
			)
			: m_ToCall(fg_Move(_ToCall))
			, m_Promise(fg_Move(_Promise))
			, m_pActorInternal(_pActorInternal)
#if DMibConfig_Concurrency_DebugBlockDestroy
			, m_ActorTypeName(_pActorInternal->m_ActorTypeName)
#endif
		{
			DMibFastCheck(m_Promise.f_IsValid());

			if (_bStoreCallStates && !_ThreadLocal.m_CrossActorStateScopes.f_IsEmpty()) [[unlikely]]
				m_State.f_StoreCallStates(_ThreadLocal);
#if DMibConfig_Concurrency_DebugActorCallstacks
			m_State.f_CaptureCallstack();
#endif
		}

		void f_Cleanup() override
		{
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
		}

		void f_Call(CConcurrencyThreadLocal &_ThreadLocal) override
		{
			auto Cleanup = g_OnScopeExit / [this]
				{
					fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
				}
			;
			f_CallShared(_ThreadLocal);
		}

		void f_CallNoDelete(CConcurrencyThreadLocal &_ThreadLocal)
		{
			f_CallShared(_ThreadLocal);
		}

		inline_always void f_CallShared(CConcurrencyThreadLocal &_ThreadLocal)
		{
			DMibFastCheck(m_pActorInternal);
			auto &PromiseThreadLocal = _ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;

			auto &State = m_State;

			auto States = State.f_RestoreCallStates();
			auto pActor = NPrivate::fg_GetInternalActor(*m_pActorInternal);

			DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == m_pActorInternal);

#if DMibConfig_Concurrency_DebugActorCallstacks
			auto Callstacks = fg_Move(State.m_Callstacks);
			NPrivate::CAsyncCallstacksScope CallstacksScope(Callstacks);
#endif
			if constexpr (mc_bIsFuture || mc_bIsAsyncGenerator)
			{
#if DMibEnableSafeCheck > 0
				PromiseThreadLocal.m_UsePromiseTypeHash = fg_GetTypeHash<t_CRet>();

				auto pPreviousUsePromiseConsumedBy = PromiseThreadLocal.m_pUsePromiseConsumedBy;
				PromiseThreadLocal.m_pUsePromiseConsumedBy = nullptr;

				auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
				PromiseThreadLocal.m_bCaptureDebugException = true;

				auto CleanupUsePromiseConsumedBy = g_OnScopeExit / [&]
					{
						PromiseThreadLocal.m_pUsePromiseConsumedBy = pPreviousUsePromiseConsumedBy;
						PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					}
				;
#endif
#ifdef DMibNeedDebugException
				try
				{
#endif
					if constexpr (mc_bIsFuture)
					{
						auto pPreviousUsePromise = PromiseThreadLocal.m_pUsePromise;
						PromiseThreadLocal.m_pUsePromise = &m_Promise;
#if DMibEnableSafeCheck > 0
						auto pPromiseData = m_Promise.f_Debug_GetData();
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
						m_Promise.f_Debug_SetCallstacks(fg_TempCopy(Callstacks));
#endif
						auto CleanupUsePromise = g_OnScopeExit / [&]
							{
								PromiseThreadLocal.m_pUsePromise = pPreviousUsePromise;
							}
						;

						auto Future = m_ToCall(*pActor, typename t_CFunctor::CIndices());
						DMibFastCheck(!PromiseThreadLocal.m_pUsePromise);
						DMibFastCheck
							(
								!Future.f_IsValid()
								||
								(
									(!PromiseThreadLocal.m_pUsePromiseConsumedBy || Future.f_Debug_HadCoroutine(PromiseThreadLocal.m_pUsePromiseConsumedBy))
									&& Future.f_Debug_HasData(pPromiseData)
								)
							)
						;

						if (PromiseThreadLocal.m_pUsePromise) [[unlikely]]
						{
							DMibPDebugBreak;
						}
					}
					else
					{
						auto Generator = m_ToCall(*pActor, typename t_CFunctor::CIndices());

						m_Promise.f_SetResult(fg_Move(Generator));
						m_Promise.f_ClearResultSet();
						m_pActorInternal = nullptr;
					}
#ifdef DMibNeedDebugException
				}
				catch (NException::CDebugException const &)
				{
					m_Promise.f_SetException(NException::fg_CurrentException());
					m_Promise.f_ClearResultSet();
					m_pActorInternal = nullptr;
				}
#endif
			}
			else
			{
				try
				{
					if constexpr (NTraits::cIsVoid<t_CRet>)
					{
						m_ToCall(*pActor, typename t_CFunctor::CIndices());
						if (!m_Promise.f_IsDiscarded())
							m_Promise.f_SetResult();

						m_Promise.f_ClearResultSet();
						m_pActorInternal = nullptr;
					}
					else
					{
						if (m_Promise.f_IsDiscarded())
							m_ToCall(*(pActor), typename t_CFunctor::CIndices());
						else
							m_Promise.f_SetResult(m_ToCall(*(pActor), typename t_CFunctor::CIndices()));

						m_Promise.f_ClearResultSet();
						m_pActorInternal = nullptr;
					}
				}
				catch (NException::CExceptionBase const &)
				{
					m_Promise.f_SetException(NException::fg_CurrentException());
					m_Promise.f_ClearResultSet();
					m_pActorInternal = nullptr;
				}
			}
		}

		~TCReportLocalFuture()
		{
			if (m_pActorInternal && m_Promise.f_IsValid())
			{
#if DMibConfig_Concurrency_DebugBlockDestroy
				m_Promise.f_SetException( DMibImpExceptionInstance(CExceptionActorDeleted, fg_Format("Actor '{}' called has been deleted", m_ActorTypeName)));
#else
				m_Promise.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted", false));
#endif
				m_Promise.f_ClearResultSet();
			}
		}

		CReportLocalState m_State;
		t_CFunctor m_ToCall;
		TCPromise<t_CRet> m_Promise;
		TCActorInternal<t_CActor> *m_pActorInternal;
#if DMibConfig_Concurrency_DebugBlockDestroy
		NStr::CStr m_ActorTypeName;
#endif
	};

	template <typename t_CReturnType>
	void NPrivate::TCFutureCoroutineContextShared<t_CReturnType>::CConcurrentRunQueueEntryImpl::f_Call(CConcurrencyThreadLocal &_ThreadLocal)
	{
		static constexpr mint c_Offset = DMibPOffsetOf(CFutureCoroutineContext, m_RunQueueImplStorage);

		CFutureCoroutineContext *pCoroutineContext = (CFutureCoroutineContext *)((uint8 *)this - c_Offset);
		auto &CoroutineContext = *pCoroutineContext;

		TCFutureCoroutineKeepAliveImplicit<t_CReturnType> KeepAlive
			{
				NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>>
				{
					fg_Attach(static_cast<NPrivate::TCPromiseData<t_CReturnType> *>(CoroutineContext.m_pPromiseData))
				}
			}
		;

		auto Handle = TCCoroutineHandle<CFutureCoroutineContext>::from_promise(CoroutineContext);

#if DMibEnableSafeCheck > 0
		DMibFastCheck(m_pResumeActor == _ThreadLocal.m_pCurrentlyProcessingActorHolder);
#endif
		this->~CConcurrentRunQueueEntryImpl();

		DMibFastCheck(CoroutineContext.m_bAwaitingInitialResume);
		CoroutineContext.m_bAwaitingInitialResume = false;

		{
			FRestoreScopesState RestoreScopes;
			CoroutineContext.f_ResumeInitialSuspend(_ThreadLocal.m_SystemThreadLocal, RestoreScopes);

			Handle.resume();
		}
	}

	template <typename t_CReturnType>
	void NPrivate::TCFutureCoroutineContextShared<t_CReturnType>::CConcurrentRunQueueEntryImpl::f_Cleanup()
	{
		static constexpr mint c_Offset = DMibPOffsetOf(CFutureCoroutineContext, m_RunQueueImplStorage);

		CFutureCoroutineContext *pCoroutineContext = (CFutureCoroutineContext *)((uint8 *)this - c_Offset);
		auto &CoroutineContext = *pCoroutineContext;

		this->~CConcurrentRunQueueEntryImpl();

		DMibFastCheck(CoroutineContext.m_bAwaitingInitialResume);

		if (CoroutineContext.m_bAwaitingInitialResume) [[likely]]
			static_cast<TCFutureCoroutineContextShared<t_CReturnType> *>(pCoroutineContext)->fp_CleanupDirectResume();
		else
		{
			TCFutureCoroutineKeepAliveImplicit<t_CReturnType> KeepAlive
				{
					NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>>
					{
						fg_Attach(static_cast<NPrivate::TCPromiseData<t_CReturnType> *>(CoroutineContext.m_pPromiseData))
					}
				}
			;
		}
	}

	template <typename t_CReturnType>
	inline_never void NPrivate::TCFutureCoroutineContextShared<t_CReturnType>::fp_CleanupDirectResume()
	{
#if DMibEnableSafeCheck > 0
		if (!fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder->f_IsHolderDestroyed())
			f_SetOwner(fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder);
#endif
		TCFutureCoroutineKeepAliveImplicit<t_CReturnType> KeepAlive
			{
				NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>>
				{
					fg_Attach(static_cast<NPrivate::TCPromiseData<t_CReturnType> *>(m_pPromiseData))
				}
			}
		;

		auto Handle = TCCoroutineHandle<TCFutureCoroutineContextShared<t_CReturnType>>::from_promise(*this);
		NPrivate::fg_DestroyCoroutine(Handle);

		auto &PromiseData = KeepAlive.f_PromiseData();

		DMibFastCheck(PromiseData.m_RefCount.f_GetPromiseCount() == 0);

		if (PromiseData.m_AfterSuspend.m_bPendingResult)
		{
			PromiseData.m_AfterSuspend.m_bPendingResult = false;
			PromiseData.f_OnResult();
		}
		else
			PromiseData.f_SetException(CAsyncResult::fs_ActorCalledDeletedException());
	}

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_CFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_CFunctor const &_pMemberFunctor
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...>
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType
		<
			typename NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>::CReturn
			, NTraits::TCDecay<tf_CPromiseParam>
		>::CType
		requires
		(
			NTraits::cIsMemberFunctionPointer<tf_CFunctor>
			&&
			!
			(
				!NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
				&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
			)
		)
	{
		using CReturn = typename NPrivate::TCGetReturnType<tf_CReturn>::CType;
		using CPromiseParam = NTraits::TCDecay<tf_CPromiseParam>;

		constexpr static bool c_bIsAsyncGenerator = NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value;
		constexpr static bool c_bIsDiscardResult = NTraits::cIsSame<CPromiseParam, CPromiseConstructDiscardResult>;
		constexpr static bool c_bIsDirectReturn = NTraits::cIsSame<CPromiseParam, CVoidTag>;
		constexpr static bool c_bIsFunctor = !c_bIsDirectReturn && !c_bIsDiscardResult && !NTraits::cIsSame<CPromiseParam, CPromiseConstructNoConsume>;

		static_assert(!c_bIsFunctor || c_bIsAsyncGenerator); // Should be caught by other fg_CallActorFutureDirectCoroutine

		auto *pActor = static_cast<tf_CContainedActor *>(NPrivate::fg_GetInternalActor(*_pActor));

		if constexpr (c_bIsAsyncGenerator)
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;

			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;

			auto Generator = (pActor->*_pMemberFunctor)(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

			static_assert(!c_bIsDirectReturn);

			if constexpr (c_bIsDiscardResult)
				return;
			else if constexpr (c_bIsFunctor)
			{
				TCAsyncResult<CReturn> Result;
				Result.f_SetResult(fg_Move(Generator));
				_PromiseParam(fg_Move(Result));
			}
			else
				return fg_Move(Generator);
		}
		else if constexpr (tf_bDirectCall)
		{
			auto Future = (pActor->*_pMemberFunctor)(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

			if constexpr (c_bIsDiscardResult)
				return;
			else
				return Future;
		}
		else
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;
			auto bPreviousSupendOnInitialSuspend = PromiseThreadLocal.m_bSupendOnInitialSuspend;
			PromiseThreadLocal.m_bSupendOnInitialSuspend = true;

			auto Cleanup = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bSupendOnInitialSuspend = bPreviousSupendOnInitialSuspend;
				}
			;

	#if DMibEnableSafeCheck > 0
			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
			PromiseThreadLocal.m_bCaptureDebugException = true;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;
	#endif
			auto Result = (pActor->*_pMemberFunctor)(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);
			DMibFastCheck(!PromiseThreadLocal.m_bSupendOnInitialSuspend); // Function calls over actor needs to co-routines

			auto &PromiseData = *Result.f_Unsafe_PromiseData();
			DMibFastCheck(PromiseData.m_BeforeSuspend.m_bIsCoroutine); // Function calls over actor needs to co-routines
			DMibFastCheck(PromiseData.m_Coroutine);

			auto &Promise = TCCoroutineHandle<NPrivate::TCFutureCoroutineContextShared<CReturn>>::from_address(PromiseData.m_Coroutine.address()).promise();
			auto pRunQueueEntry = Promise.f_ConstructRunQueueEntry();
#if DMibEnableSafeCheck > 0
			pRunQueueEntry->m_pResumeActor = _pActor;
#endif
			DMibFastCheck(Promise.m_bAwaitingInitialResume);

			CConcurrentRunQueueEntryHolder EntryHolder(pRunQueueEntry);

			_pActor->f_QueueProcessEntry
				(
					fg_Move(EntryHolder)
					, ConcurrencyThreadLocal
				)
			;

			DMibFastCheck(EntryHolder.f_IsEmpty());

			if constexpr (c_bIsDiscardResult)
				return;
			else
				return Result;
		}
	}

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_CFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_CFunctor const &_pMemberFunctor
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...>
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType
		<
			typename NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>::CReturn
			, NTraits::TCDecay<tf_CPromiseParam>
		>::CType
		requires
		(
			NTraits::cIsMemberFunctionPointer<tf_CFunctor>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
			&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
		)
	{
		using CReturn = typename NPrivate::TCGetReturnType<tf_CReturn>::CType;

		TCFutureOnResult<CReturn> fOnResultSet = fg_Forward<tf_CPromiseParam>(_PromiseParam);

		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;

		auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
		PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

		auto CleanupOnResultSet = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
			}
		;

#if DMibEnableSafeCheck > 0
		auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
		PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

		auto CleanupOnResultSetConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
			}
		;

		PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<CReturn>();
#endif
		auto *pActor = static_cast<tf_CContainedActor *>(NPrivate::fg_GetInternalActor(*_pActor));

		if constexpr (tf_bDirectCall)
		{
			auto Future = (pActor->*_pMemberFunctor)(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

#if DMibEnableSafeCheck > 0
			DMibFastCheck(!PromiseThreadLocal.m_pOnResultSet);
			DMibFastCheck(Future.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
#endif
			return;
		}
		else
		{
			auto bPreviousSupendOnInitialSuspend = PromiseThreadLocal.m_bSupendOnInitialSuspend;
			PromiseThreadLocal.m_bSupendOnInitialSuspend = true;

			auto Cleanup = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bSupendOnInitialSuspend = bPreviousSupendOnInitialSuspend;
				}
			;

	#if DMibEnableSafeCheck > 0
			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
			PromiseThreadLocal.m_bCaptureDebugException = true;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;
	#endif
			auto Result = (pActor->*_pMemberFunctor)(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

#if DMibEnableSafeCheck > 0
			DMibFastCheck(!PromiseThreadLocal.m_bSupendOnInitialSuspend); // Function calls over actor needs to co-routines
			DMibFastCheck(!PromiseThreadLocal.m_pOnResultSet);
			DMibFastCheck(Result.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
#endif
			auto &PromiseData = *Result.f_Unsafe_PromiseData();
			DMibFastCheck(PromiseData.m_BeforeSuspend.m_bIsCoroutine); // Function calls over actor needs to co-routines
			DMibFastCheck(PromiseData.m_Coroutine);

			auto &Promise = TCCoroutineHandle<NPrivate::TCFutureCoroutineContextShared<CReturn>>::from_address(PromiseData.m_Coroutine.address()).promise();
			auto pRunQueueEntry = Promise.f_ConstructRunQueueEntry();
#if DMibEnableSafeCheck > 0
			pRunQueueEntry->m_pResumeActor = _pActor;
#endif
			DMibFastCheck(Promise.m_bAwaitingInitialResume);

			CConcurrentRunQueueEntryHolder EntryHolder(pRunQueueEntry);

			_pActor->f_QueueProcessEntry
				(
					fg_Move(EntryHolder)
					, ConcurrencyThreadLocal
				)
			;

			DMibFastCheck(EntryHolder.f_IsEmpty());

			return;
		}
	}

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_CFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_CFunctor const &_pFunctionPointer
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...>
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType
		<
			typename NTraits::TCFunctionTraits<NTraits::TCRemovePointer<tf_CFunctor>>::CReturn
			, NTraits::TCDecay<tf_CPromiseParam>
		>::CType
		requires
		(
			!NTraits::cIsMemberFunctionPointer<tf_CFunctor>
			&&
			!
			(
				!NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
				&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
				&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
			)
		)
	{
		using CReturn = typename NPrivate::TCGetReturnType<tf_CReturn>::CType;
		using CPromiseParam = NTraits::TCDecay<tf_CPromiseParam>;

		constexpr static bool c_bIsAsyncGenerator = NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value;
		constexpr static bool c_bIsDiscardResult = NTraits::cIsSame<CPromiseParam, CPromiseConstructDiscardResult>;
		constexpr static bool c_bIsDirectReturn = NTraits::cIsSame<CPromiseParam, CVoidTag>;
		constexpr static bool c_bIsFunctor = !c_bIsDirectReturn && !c_bIsDiscardResult && !NTraits::cIsSame<CPromiseParam, CPromiseConstructNoConsume>;

		static_assert(!c_bIsFunctor || c_bIsAsyncGenerator); // Should be caught by other fg_CallActorFutureDirectCoroutine

		if constexpr (c_bIsAsyncGenerator)
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;

			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;

			auto Generator = _pFunctionPointer(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

			static_assert(!c_bIsDirectReturn);

			if constexpr (c_bIsDiscardResult)
				return;
			else if constexpr (c_bIsFunctor)
			{
				TCAsyncResult<CReturn> Result;
				Result.f_SetResult(fg_Move(Generator));
				_PromiseParam(fg_Move(Result));
			}
			else
				return fg_Move(Generator);
		}
		else if constexpr (tf_bDirectCall)
		{
			auto Future = _pFunctionPointer(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

			if constexpr (c_bIsDiscardResult)
				return;
			else
				return Future;
		}
		else
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;
			auto bPreviousSupendOnInitialSuspend = PromiseThreadLocal.m_bSupendOnInitialSuspend;
			PromiseThreadLocal.m_bSupendOnInitialSuspend = true;

			auto Cleanup = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bSupendOnInitialSuspend = bPreviousSupendOnInitialSuspend;
				}
			;

	#if DMibEnableSafeCheck > 0
			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
			PromiseThreadLocal.m_bCaptureDebugException = true;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;
	#endif
			auto Result = _pFunctionPointer(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);
			DMibFastCheck(!PromiseThreadLocal.m_bSupendOnInitialSuspend); // Function calls over actor needs to co-routines

			auto &PromiseData = *Result.f_Unsafe_PromiseData();
			DMibFastCheck(PromiseData.m_BeforeSuspend.m_bIsCoroutine); // Function calls over actor needs to co-routines
			DMibFastCheck(PromiseData.m_Coroutine);

			auto &Promise = TCCoroutineHandle<NPrivate::TCFutureCoroutineContextShared<CReturn>>::from_address(PromiseData.m_Coroutine.address()).promise();
			auto pRunQueueEntry = Promise.f_ConstructRunQueueEntry();
#if DMibEnableSafeCheck > 0
			pRunQueueEntry->m_pResumeActor = _pActor;
#endif
			DMibFastCheck(Promise.m_bAwaitingInitialResume);

			CConcurrentRunQueueEntryHolder EntryHolder(pRunQueueEntry);

			_pActor->f_QueueProcessEntry
				(
					fg_Move(EntryHolder)
					, ConcurrencyThreadLocal
				)
			;

			DMibFastCheck(EntryHolder.f_IsEmpty());

			if constexpr (c_bIsDiscardResult)
				return;
			else
				return Result;
		}
	}

	template
	<
		bool tf_bDirectCall
		, typename tf_CReturn
		, typename tf_CContainedActor
		, typename tf_CActor
		, typename tf_CFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CFunctionParams
		, typename ...tfp_CParams
	>
	auto fg_CallActorFutureDirectCoroutine
		(
			tf_CActor *_pActor
			, tf_CFunctor const &_pFunctionPointer
			, tf_CPromiseParam &&_PromiseParam
			, NMeta::TCTypeList<tfp_CFunctionParams...>
			, tfp_CParams && ...p_Params
		)
		-> typename NPrivate::TCCallActorGetReturnType
		<
			typename NTraits::TCFunctionTraits<NTraits::TCRemovePointer<tf_CFunctor>>::CReturn
			, NTraits::TCDecay<tf_CPromiseParam>
		>::CType
		requires
		(
			!NTraits::cIsMemberFunctionPointer<tf_CFunctor>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructDiscardResult>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CPromiseConstructNoConsume>
			&& !NTraits::cIsSame<NTraits::TCDecay<tf_CPromiseParam>, CVoidTag>
			&& !NPrivate::TCIsAsyncGenerator<tf_CReturn>::mc_Value
		)
	{
		using CReturn = typename NPrivate::TCGetReturnType<tf_CReturn>::CType;

		TCFutureOnResult<CReturn> fOnResultSet = fg_Forward<tf_CPromiseParam>(_PromiseParam);

		auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
		auto &PromiseThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal;

		auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
		PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

		auto CleanupOnResultSet = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
			}
		;

#if DMibEnableSafeCheck > 0
		auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
		PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

		auto CleanupOnResultSetConsumedBy = g_OnScopeExit / [&]
			{
				PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
			}
		;

		PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<CReturn>();
#endif
		if constexpr (tf_bDirectCall)
		{
			auto Future = _pFunctionPointer(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

#if DMibEnableSafeCheck > 0
			DMibFastCheck(!PromiseThreadLocal.m_pOnResultSet);
			DMibFastCheck(Future.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
#endif
			return;
		}
		else
		{
			auto bPreviousSupendOnInitialSuspend = PromiseThreadLocal.m_bSupendOnInitialSuspend;
			PromiseThreadLocal.m_bSupendOnInitialSuspend = true;

			auto Cleanup = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bSupendOnInitialSuspend = bPreviousSupendOnInitialSuspend;
				}
			;

	#if DMibEnableSafeCheck > 0
			auto pPreviousCurrentActorCalled = PromiseThreadLocal.m_pCurrentActorCalled;
			PromiseThreadLocal.m_pCurrentActorCalled = _pActor;

			auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
			PromiseThreadLocal.m_bCaptureDebugException = true;

			auto CleanupDebug = g_OnScopeExit / [&]
				{
					PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					PromiseThreadLocal.m_pCurrentActorCalled = pPreviousCurrentActorCalled;
				}
			;
	#endif
			auto Result = _pFunctionPointer(fg_CopyOrMove<tfp_CFunctionParams>(fg_Forward<tfp_CParams>(p_Params))...);

#if DMibEnableSafeCheck > 0
			DMibFastCheck(!PromiseThreadLocal.m_bSupendOnInitialSuspend); // Function calls over actor needs to co-routines
			DMibFastCheck(!PromiseThreadLocal.m_pOnResultSet);
			DMibFastCheck(Result.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
#endif
			auto &PromiseData = *Result.f_Unsafe_PromiseData();
			DMibFastCheck(PromiseData.m_BeforeSuspend.m_bIsCoroutine); // Function calls over actor needs to co-routines
			DMibFastCheck(PromiseData.m_Coroutine);

			auto &Promise = TCCoroutineHandle<NPrivate::TCFutureCoroutineContextShared<CReturn>>::from_address(PromiseData.m_Coroutine.address()).promise();
			auto pRunQueueEntry = Promise.f_ConstructRunQueueEntry();
#if DMibEnableSafeCheck > 0
			pRunQueueEntry->m_pResumeActor = _pActor;
#endif
			DMibFastCheck(Promise.m_bAwaitingInitialResume);

			CConcurrentRunQueueEntryHolder EntryHolder(pRunQueueEntry);

			_pActor->f_QueueProcessEntry
				(
					fg_Move(EntryHolder)
					, ConcurrencyThreadLocal
				)
			;

			DMibFastCheck(EntryHolder.f_IsEmpty());

			return;
		}
	}

	namespace NPrivate
	{
		template <typename tf_CParam>
		auto fg_ConvertToFuture(tf_CParam &&_Param)
			requires (NPrivate::TCIsFuture<NTraits::TCRemoveReferenceAndQualifiers<tf_CParam>>::mc_Value)
		{
			return fg_Forward<tf_CParam>(_Param);
		}

		template <typename tf_CParam>
		auto fg_ConvertToFuture(tf_CParam &&_Param)
		{
			return fg_Forward<tf_CParam>(_Param).f_Future();
		}

		template <typename ...tfp_CParams, mint ...tfp_Indidies>
		auto fg_AnyDoneImpl(NMeta::TCIndices<tfp_Indidies...> const &, TCFuture<tfp_CParams> && ...p_Futures)
			-> TCFuture<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>>
		{
			struct CState final
			{
				TCPromiseFuturePair<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>> m_Promise;
				NAtomic::CAtomicFlag m_bReplied;
			};

			NStorage::TCSharedPointer<CState> pState = fg_Construct();

			(
				[&]
				{
					fg_Move(p_Futures).f_OnResultSet
						(
							[pState](TCAsyncResult<tfp_CParams> &&_Result)
							{

								if (!pState->m_bReplied.f_TestAndSet())
								{
									if (!_Result)
										pState->m_Promise.m_Promise.f_SetException(fg_Move(_Result));
									else
									{
										NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...> Results;
										fg_Get<tfp_Indidies>(Results) = fg_Move(*_Result);
										pState->m_Promise.m_Promise.f_SetResult(fg_Move(Results));
									}

									pState->m_Promise.m_Promise.f_ClearResultSet();
								}
							}
						)
					;
				}
				()
				, ...
			);

			return fg_Move(pState->m_Promise.m_Future);
		}

		template <typename ...tfp_CParams, mint ...tfp_Indidies>
		auto fg_AnySuccessImpl(NMeta::TCIndices<tfp_Indidies...> const &, TCFuture<tfp_CParams> && ...p_Futures)
			-> TCFuture<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>>
		{
			static constexpr mint c_nParams = sizeof...(tfp_CParams);

			struct CState
			{
				TCPromiseFuturePair<NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...>> m_Promise;
				NException::CExceptionPointer m_Errors[c_nParams];
				NAtomic::CAtomicFlag m_bReplied;
				NAtomic::TCAtomic<mint> m_nErrors;
			};

			NStorage::TCSharedPointer<CState> pState = fg_Construct();

			(
				[&]
				{
					fg_Move(p_Futures).f_OnResultSet
						(
							[pState](TCAsyncResult<tfp_CParams> &&_Result)
							{
								auto &State = *pState;
								if (!_Result)
								{
									State.m_Errors[tfp_Indidies] = _Result.f_ExceptionPointer();
									if (State.m_nErrors.f_FetchAdd(1) == c_nParams && !State.m_bReplied.f_TestAndSet())
									{
										NContainer::TCVector<NException::CExceptionPointer> Exceptions(State.m_Errors, c_nParams);
										State.m_Promise.m_Promise.f_SetException(DMibErrorInstanceExceptionVector("All operations failed", fg_Move(Exceptions), false));
										State.m_Promise.m_Promise.f_ClearResultSet();
									}
								}
								else
								{
									if (!State.m_bReplied.f_TestAndSet())
									{
										NStorage::TCTuple<NStorage::TCOptional<typename TCConvertResultTypeVoid<tfp_CParams>::CType>...> Results;
										fg_Get<tfp_Indidies>(Results) = fg_Move(*_Result);
										State.m_Promise.m_Promise.f_SetResult(fg_Move(Results));
										State.m_Promise.m_Promise.f_ClearResultSet();
									}
								}
							}
						)
					;
				}
				()
				, ...
			);

			return fg_Move(pState->m_Promise.m_Future);
		}
	}

	template <typename ...tfp_CParams>
	auto fg_AnyDone(tfp_CParams && ...p_Params)
	{
		return NPrivate::fg_AnyDoneImpl(NMeta::TCConsecutiveIndices<sizeof...(tfp_CParams)>(), NPrivate::fg_ConvertToFuture(fg_Forward<tfp_CParams>(p_Params))...);
	}

	template <typename ...tfp_CParams>
	auto fg_AnySuccess(tfp_CParams && ...p_Params)
	{
		return NPrivate::fg_AnySuccessImpl(NMeta::TCConsecutiveIndices<sizeof...(tfp_CParams)>(), NPrivate::fg_ConvertToFuture(fg_Forward<tfp_CParams>(p_Params))...);
	}
}

#include "Malterlib_Concurrency_ActorCallCoroutine.hpp"
