// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/Actor/Timer>

namespace NMib::NConcurrency
{
	CCurrentActorScope::CCurrentActorScope(TCActor<CActor> const &_Actor)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pLastActor = ThreadLocal.m_pCurrentActor;
		ThreadLocal.m_pCurrentActor = _Actor->fp_GetActor();
	}

	CCurrentActorScope::CCurrentActorScope(CActor const *_pActor)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mp_pLastActor = ThreadLocal.m_pCurrentActor;
		ThreadLocal.m_pCurrentActor = const_cast<CActor *>(_pActor);
	}

	CCurrentActorScope::~CCurrentActorScope()
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		ThreadLocal.m_pCurrentActor = mp_pLastActor;
	}

	namespace NPrivate
	{
		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto CThisActor::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
		{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith
					<
						typename NTraits::TCAddPointer<typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType>::CFunctionType>::CType
						, void (tfp_CCallParams...)
					>::mc_Value
					, "Invalid params for function"
				)
			;
#endif

			using CMemberFunction = typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType;
			using CActorClass = typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass;

			TCActorInternal<CActorClass> *pActor = (TCActorInternal<CActorClass> *)m_pThis;
			DMibRequire(dynamic_cast<CActorClass *>(pActor->fp_GetActor()));
			DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
			TCActor<CActorClass> Actor{fg_Explicit(pActor)};

			return TCActorCall
				<
					TCActor<CActorClass>
					, CMemberFunction
					, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
					, NMeta::TCTypeList<tfp_CCallParams...>
					, true
				>
				(
					Actor
					, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
					, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
				)
			;
		}

		template <typename t_CTypeList>
		struct TCActorCallWithParamsTuple
		{
		};

		template <typename ...tp_CParam>
		struct TCActorCallWithParamsTuple<NMeta::TCTypeList<tp_CParam...>>
		{
			using CType = NStorage::TCTuple<typename NTraits::TCRemoveReferenceAndQualifiers<tp_CParam>::CType...>;
		};

		template <typename t_FMemberFunctionPointer, bool t_bDirectCall>
		struct TCActorCallWithParams
		{
			using CMemberPointerTypes = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CParams;

			static constexpr mint mc_nParams = NMeta::TCTypeList_Len<CMemberPointerTypes>::mc_Value;
			using CReturnType = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CReturn;
			using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CClass;
			using CIndices = typename NMeta::TCMakeConsecutiveIndices<mc_nParams>::CType;

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
#if DMibEnableSafeCheck > 0
			inline_never
#else
			mark_artificial inline_always
#endif
			CReturnType operator() (tf_CActor &_Actor, NMeta::TCIndices<tfp_Indices...>)
			{
				return (_Actor.*m_pMemberPointer)(fg_Move(fg_Get<tfp_Indices>(m_Params))...);
			}

			constexpr bool f_IsDispatch() const
			{
				return NTraits::TCIsSame<typename NTraits::TCMemberFunctionPointerTraits<t_FMemberFunctionPointer>::CClass, CActor>::mc_Value;
			}

			t_FMemberFunctionPointer m_pMemberPointer;
			typename TCActorCallWithParamsTuple<CMemberPointerTypes>::CType m_Params;
			static constexpr bool mc_bDirectCall = t_bDirectCall;

		private:
			TCActorCallWithParams(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams &&);
		};

		template <bool tf_bDirectCall, typename tf_FMemberFunction, typename... tfp_CParams, mint... tfp_Indicies>
		auto fg_BindTupleHelper(tf_FMemberFunction _pToBind, NStorage::TCTuple<tfp_CParams...> &&_Params, NMeta::TCIndices<tfp_Indicies...>)
		{
			return TCActorCallWithParams
				<
					tf_FMemberFunction
					, tf_bDirectCall
				>
				(
					_pToBind
					, fg_Forward<tfp_CParams>(fg_Get<tfp_Indicies>(_Params))...
				)
			;
		}

		template <bool tf_bDirectCall, typename tf_CTypeList, typename tf_FMemberFunction, typename... tfp_CParams>
		auto fg_BindHelper(tf_FMemberFunction _pToBind, NStorage::TCTuple<tfp_CParams...> &&_Params)
		{
			return fg_BindTupleHelper<tf_bDirectCall>
				(
					_pToBind
					, fg_Move(_Params)
					, typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType()
				)
			;
		}

		template <bool tf_bDirectCall, typename tf_FMemberFunction, typename... tfp_CParams, mint... tfp_Indicies>
		auto fg_BindConstructHelper(tf_FMemberFunction _pToBind, TCConstruct<void, tfp_CParams...> &&_Params, NMeta::TCIndices<tfp_Indicies...>)
		{
			return TCActorCallWithParams
				<
					tf_FMemberFunction
					, tf_bDirectCall
				>
				(
					_pToBind
					, fg_Forward<tfp_CParams>(fg_Get<tfp_Indicies>(_Params.m_Params))...
				)
			;
		}

		template <bool tf_bDirectCall, typename tf_CTypeList, typename tf_FMemberFunction, typename... tfp_CParams>
		auto fg_BindHelper(tf_FMemberFunction _pToBind, TCConstruct<void, tfp_CParams...> &&_Params)
		{
			return fg_BindConstructHelper<tf_bDirectCall>
				(
					_pToBind
					, fg_Move(_Params)
					, typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType()
				)
			;
		}

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
			return fg_ToTupleByValueHelper(fg_Move(_Params), typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType());
		}


	}

	template <typename t_CActor, typename t_CFunctor>
	struct TCActorResultCall
	{
		static_assert(!NTraits::TCIsReference<t_CActor>::mc_Value, "Incorrect type");
		static_assert(!NTraits::TCIsReference<t_CFunctor>::mc_Value, "Incorrect type");

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

	template <typename t_CType>
	struct TCIsActorResultCallHelper : public NTraits::TCCompileTimeConstant<bool, false>
	{
	};

	template <typename t_CActor, typename t_CFunctor>
	struct TCIsActorResultCallHelper<TCActorResultCall<t_CActor, t_CFunctor>> : public NTraits::TCCompileTimeConstant<bool, true>
	{
	};

	template <typename t_CType>
	struct TCIsActorResultCall : public TCIsActorResultCallHelper<typename NTraits::TCRemoveReference<t_CType>::CType>
	{
	};

	namespace NPrivate
	{
		template <typename t_CReturnType>
		struct TCCallSyncState
		{
			NThread::CEvent m_WaitEvent;
			TCAsyncResult<t_CReturnType> m_Result;

			template <mint ...tfp_Indices, typename ...tfp_CResults>
			void f_TransferResults(NMeta::TCIndices<tfp_Indices...> const &_Indicies, tfp_CResults && ...p_Results)
			{
				t_CReturnType Result;

				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!p_Results)
							{
								if (!m_Result.f_IsSet())
									m_Result.f_SetException(p_Results);
							}
							else
								fg_Get<tfp_Indices>(Result) = fg_Move(*p_Results);
							return false;
						}
						()...
					}
				;
				(void)Dummy;

				if (!m_Result.f_IsSet())
					m_Result.f_SetResult(fg_Move(Result));

				m_WaitEvent.f_SetSignaled();
			}
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

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	struct TCActorCallPackAwaiter
	{
		using CUnwrappedType = NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CCalls::CReturnType>::CType...>;
		using CWrappedType = NStorage::TCTuple<TCAsyncResult<typename tp_CCalls::CReturnType>...>;

		TCActorCallPackAwaiter(NStorage::TCTuple<tp_CCalls...> &&_Calls, t_FExceptionTransform &&_fExceptionTransform);
		~TCActorCallPackAwaiter();

		template
		<
			typename tf_CResultFunctor
			, mint... tfp_Indices
		>
		void fp_ActorCall(tf_CResultFunctor &&_ResultFunctor, TCActor<> const &_ResultActor, NMeta::TCIndices<tfp_Indices...>);

		template <mint... tfp_Indices>
		bool fp_Unwrap(CUnwrappedType &o_Results, NStr::CStr &o_Errors, NContainer::TCVector<NException::CExceptionPointer> &o_Exceptions, NMeta::TCIndices<tfp_Indices...>);

		template <mint... tfp_Indices>
		void fp_TransformExceptions(CWrappedType &o_Results, NMeta::TCIndices<tfp_Indices...>);

		bool await_ready() const noexcept;
		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle);
#if DMibEnableSafeCheck > 0
		auto await_resume();
#else
		auto await_resume() noexcept(!t_bUnwrap);
#endif

	private:
		CWrappedType mp_Results;
		NStorage::TCTuple<tp_CCalls...> mp_Calls;
		NAtomic::TCAtomic<mint> mp_ResultAvailable;
		/*[[no_unique_address]]*/ t_FExceptionTransform mp_fExceptionTransform;
#if DMibEnableSafeCheck > 0
		NException::CExceptionPointer mp_pDebugException;
#endif
	};

	template <typename t_CActorCall, typename... tp_CCalls>
	struct [[nodiscard]] TCActorCallPackWithError : protected CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCActorCallPackWithError *m_pWrapped;
			auto operator co_await() &&;
		};

		TCActorCallPackWithError(t_CActorCall *_pActorCall, NStr::CStr const &_Error);

		auto operator co_await() &&;
		CNoUnwrapAsyncResult f_Wrap() &&;

	private:

		t_CActorCall *mp_pActorCall;
	};

	template <typename... tp_CCalls>
	struct [[nodiscard]] TCActorCallPack
	{
		NStorage::TCTuple<tp_CCalls...> m_Calls;

		TCActorCallPack(NStorage::TCTuple<tp_CCalls...> &&_Tuple)
			: m_Calls(fg_Move(_Tuple))
		{
		}

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		TCActorCallPack<tp_CCalls..., TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall>>
		operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_OtherCall)
		{
			return NStorage::fg_TupleConcatenate(fg_Move(m_Calls), NStorage::fg_Tuple(fg_Move(_OtherCall)));
		}

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> &&_Future) &&;

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
		{
			fg_Move(*this).fp_ActorCall(fg_Move(_ResultCall), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());
		}

		template <typename tf_CFunctor, TCEnableIfType<!TCIsActorResultCall<tf_CFunctor>::mc_Value> * = nullptr>
		void operator > (tf_CFunctor &&_Functor) &&
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			fg_Move(*this).fp_ActorCall(pActor / fg_Forward<tf_CFunctor>(_Functor), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());
		}

		auto f_CallSync(fp64 _Timeout = -1.0) &&
		{
			return fg_Move(*this).fp_CallSync(_Timeout);
		}

		struct CNoUnwrapAsyncResult
		{
			TCActorCallPack *m_pWrapped;
			auto operator co_await() &&
			{
				return TCActorCallPackAwaiter<false, void *, tp_CCalls...>{fg_Move(m_pWrapped->m_Calls), nullptr};
			}
		};

		CNoUnwrapAsyncResult f_Wrap() &&
		{
			return {this};
		}

		auto operator co_await() &&
		{
			return TCActorCallPackAwaiter<true, void *, tp_CCalls...>{fg_Move(m_Calls), nullptr};
		}

		auto operator % (NStr::CStr const &_ErrorString) && -> TCActorCallPackWithError<TCActorCallPack, tp_CCalls...>
		{
			return {this, _ErrorString};
		}

		auto operator ^ (NStr::CStr const &_ErrorString) && -> TCActorCallPackWithError<TCActorCallPack, tp_CCalls...>
		{
			return {this, _ErrorString};
		}

	private:
		NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CCalls::CReturnType>::CType...> fp_CallSync(fp64 _Timeout) &&
		{
			using CReturnType = NStorage::TCTuple<typename NPrivate::TCConvertResultTypeVoid<typename tp_CCalls::CReturnType>::CType...>;
			NStorage::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType>> pResult = fg_Construct();
			fg_Move(*this) > NPrivate::fg_DirectResultActor() / [pResult](auto &&...p_Results)
				{
					pResult->f_TransferResults(typename NMeta::TCMakeConsecutiveIndices<sizeof...(p_Results)>::CType(), fg_Forward<decltype(p_Results)>(p_Results)...);
				}
			;

			if (_Timeout >= 0.0)
			{
				if (pResult->m_WaitEvent.f_WaitTimeout(_Timeout))
				{
					NStr::CStr Names;
					TCInitializerList<bool> Dummy =
						{
							[&]
							{
								NStr::fg_AddStrSep(Names, fg_GetTypeName<typename tp_CCalls::CFunctor>(), ",");
								return false;
							}
							()...
						}
					;
					(void)Dummy;
					DMibError(NStr::fg_Format("Timed out waiting for synchronous actor calls to '{}' to finish", Names));
				}
			}
			else
				pResult->m_WaitEvent.f_Wait();

			return pResult->m_Result.f_Move();
		}

		template <typename tf_CResultActor, typename tf_CResultFunctor, mint... tfp_Indices>
		void fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>) &&;
	};

	namespace NPrivate
	{
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultActor, typename tf_CResultFunctor, bool tf_bDirectCall>
		bool fg_CallActorInternal
			(
				TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &_ActorCall
				, TCActor<tf_CResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		{
			typename tf_CActor::CNonWeak Actor;

			static constexpr bool c_bIsDirect = NTraits::TCIsSame<tf_CResultActor, NPrivate::CDirectResultActor>::mc_Value;

			if constexpr (tf_CActor::mc_bIsWeak)
			{
				Actor = _ActorCall.mp_Actor.f_Lock();
				if (!Actor)
				{
					if (NTraits::TCIsSame<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CResultFunctor>::CType, NPrivate::CDiscardResultFunctor>::mc_Value)
						return true;

					if constexpr (c_bIsDirect)
					{
						TCAsyncResult<typename TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall>::CReturnType> Result;
						Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted"));
						NPrivate::fg_CallResultFunctorDirect(_fResultFunctor, fg_Move(Result));
						return true;
					}
					else
					{
						_ResultActor.f_GetRealActor()->f_QueueProcess
							(
								[ResultActor = fg_Move(_ResultActor), fResultFunctor = fg_Move(_fResultFunctor)]() mutable
								{
									TCAsyncResult<typename TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall>::CReturnType> Result;
									Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted"));
									NPrivate::fg_CallResultFunctor(fResultFunctor, ResultActor->fp_GetActor(), fg_Move(Result));
								}
							)
						;
						return true;
					}
				}
#if DMibEnableSafeCheck > 0
				using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>;
#endif

				// If you get this, use f_CallActor instead of calling directly
				DMibFastCheck((NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value)  || !Actor->f_GetDistributedActorData() || Actor->f_GetDistributedActorData()->f_IsValidForCall());
			}
			else
				Actor = _ActorCall.mp_Actor;

			Actor.f_GetRealActor()->template f_Call<typename tf_CActor::CContainedActor, tf_CFunctor>
				(
					NPrivate::fg_BindHelper<tf_bDirectCall, tf_CTypeList>
					(
						fg_Move(_ActorCall.mp_Functor)
						, fg_Move(_ActorCall.mp_Params)
					)
					, fg_Move(_ResultActor)
					, fg_Move(_fResultFunctor)
				)
			;
			return true;
		}
	}

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform = void *>
	struct TCActorCallAwaiter
	{
		TCActorCallAwaiter(t_CActorCall &&_ActorCall, t_FExceptionTransform &&_fExceptionTransform = nullptr);
		~TCActorCallAwaiter();

		bool await_ready() const noexcept;
		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle);
#if DMibEnableSafeCheck > 0
		auto await_resume();
#else
		auto await_resume() noexcept(!t_bUnwrap);
#endif

	private:
		t_CActorCall mp_ActorCall;
		TCAsyncResult<t_CReturnType> mp_Result;
		NAtomic::TCAtomic<mint> mp_ResultAvailable;
		/*[[no_unique_address]]*/ t_FExceptionTransform mp_fExceptionTransform;
#if DMibEnableSafeCheck > 0
		NException::CExceptionPointer mp_pDebugException;
#endif
	};

	template <typename t_CActorCall, typename t_CReturnType>
	struct [[nodiscard]] TCActorCallWithError : protected CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCActorCallWithError *m_pWrapped;
			auto operator co_await() &&;
		};

		TCActorCallWithError(t_CActorCall *_pActorCall, NStr::CStr const &_Error);

		auto operator co_await() &&;
		CNoUnwrapAsyncResult f_Wrap() &&;
		TCFutureWithError<t_CReturnType> f_Future() &&;

	private:

		t_CActorCall *mp_pActorCall;
	};

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool t_bDirectCall>
	struct [[nodiscard]] TCActorCall
	{
		static_assert(!NTraits::TCIsReference<t_CActor>::mc_Value, "Incorrect type");
		static_assert(!NTraits::TCIsReference<t_CFunctor>::mc_Value, "Incorrect type");
		static_assert(!NTraits::TCIsReference<t_CParams>::mc_Value, "Incorrect type");

		using CReturnType = typename NPrivate::TCGetReturnType<typename NTraits::TCMemberFunctionPointerTraits<t_CFunctor>::CReturn>::CType;
		using CTypeList = t_CTypeList;
		using CFunctor = t_CFunctor;

		TCActorCall(t_CActor const &_Actor, t_CFunctor const &_Functor, t_CParams &&_Params)
			: mp_Actor(_Actor)
			, mp_Functor(_Functor)
			, mp_Params(fg_Move(_Params))
		{
		}

		TCActorCall(t_CActor &&_Actor, t_CFunctor &&_Functor, t_CParams &&_Params)
			: mp_Actor(fg_Move(_Actor))
			, mp_Functor(fg_Move(_Functor))
			, mp_Params(fg_Move(_Params))
		{
		}

		TCActorCall(TCActorCall const &_Other) = default;
		TCActorCall(TCActorCall &&_Other) = default;

#ifdef DMibContractConfigure_CheckEnabled
		~TCActorCall()
		{
			DMibCheck(NException::fg_UncaughtExceptions() || mp_Actor.f_IsEmpty())("Actor call without result used");
		}
#endif

		void f_Clear()
		{
			mp_Actor.f_Clear();
		}

		auto f_ByValue()
		{
			return TCActorCall<t_CActor, t_CFunctor, decltype(NPrivate::fg_ToTupleByValue(fg_Move(mp_Params))), t_CTypeList, t_bDirectCall>
				{
					fg_Move(mp_Actor)
					, fg_Move(mp_Functor)
					, NPrivate::fg_ToTupleByValue(fg_Move(mp_Params))
				}
			;
		}

		TCFuture<CReturnType> f_Future() &&
		{
			return fg_Move(*this);
		}

		TCActorCall &operator =(TCActorCall const &_Other)
		{
			mp_Actor = _Other.mp_Actor;
			mp_Functor = _Other.mp_Functor;
			mp_Params = _Other.mp_Params;
		}

		TCActorCall &operator =(TCActorCall &&_Other)
		{
			mp_Actor = fg_Move(_Other.mp_Actor);
			mp_Functor = fg_Move(_Other.mp_Functor);
			mp_Params = fg_Move(_Other.mp_Params);
		}

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_OtherCall) &&
			-> TCActorCallPack<TCActorCall, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall>>
		{
			return TCActorCallPack<TCActorCall, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall>>
				(NStorage::fg_Tuple(fg_Move(*this), fg_Move(_OtherCall)))
			;
		}

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> &&_Future) &&;

		template
		<
			typename tf_CFunctor
			, TCEnableIfType
			<
				!TCIsActorResultCall<tf_CFunctor>::mc_Value
				&& !NPrivate::TCIsPromise<typename NTraits::TCRemoveReference<tf_CFunctor>::CType>::mc_Value
				&& !NPrivate::TCIsPromiseWithError<typename NTraits::TCRemoveReference<tf_CFunctor>::CType>::mc_Value
			> * = nullptr
		>
		void operator > (tf_CFunctor &&_Functor) &&
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			fg_Move(*this) > pActor / fg_Forward<tf_CFunctor>(_Functor);
		}

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
		{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<CReturnType> &&)>::mc_Value
					, "Incorrect type in result call"
				)
			;
#endif
			DMibFastCheck(!mp_Actor.f_IsEmpty());
#ifdef DMibContractConfigure_CheckEnabled
			auto Cleanup = g_OnScopeExit > [&]
				{
					mp_Actor.f_Clear();
				}
			;

#endif
			NPrivate::fg_CallActorInternal(*this, fg_Move(_ResultCall.mp_Actor), _ResultCall.mp_Functor);
		}

		template <typename tf_CResult>
		void operator > (TCPromise<tf_CResult> const &_Promise) &&
		{
			fg_Move(*this) > NPrivate::fg_DirectResultActor() / [_Promise](TCAsyncResult<tf_CResult> &&_Result)
				{
					_Promise.f_SetResult(fg_Move(_Result));
				}
			;
		}

		template <typename tf_CResult>
		void operator > (TCPromiseWithError<tf_CResult> const &_PromiseWithError) &&
		{
			fg_Move(*this) > NPrivate::fg_DirectResultActor() /
				(
					_PromiseWithError / [Promise = _PromiseWithError.m_Promise](tf_CResult &&_Result)
					{
						Promise.f_SetResult(fg_Move(_Result));
					}
				)
			;
		}

		void operator > (TCPromiseWithError<void> const &_PromiseWithError) &&
		{
			fg_Move(*this) > NPrivate::fg_DirectResultActor() /
				(
					_PromiseWithError / [Promise = _PromiseWithError.m_Promise]()
					{
						Promise.f_SetResult();
					}
				)
			;
		}

		auto operator % (NStr::CStr const &_ErrorString) && -> TCActorCallWithError<TCActorCall, CReturnType>
		{
			return {this, _ErrorString};
		}

		TCFuture<CReturnType> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true) &&;

		auto f_CallSync() &&
		{
			TCAsyncResult<CReturnType> Result;
			NThread::CEvent WaitEvent;
			fg_Move(*this) > NPrivate::fg_DirectResultActor() / [&](TCAsyncResult<CReturnType> &&_Result)
				{
					Result = fg_Move(_Result);
					WaitEvent.f_SetSignaled();
				}
			;

			WaitEvent.f_Wait();

			return Result.f_Move();
		}

		auto f_CallSync(fp64 _Timeout) &&
		{
			NStorage::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType>> pResult = fg_Construct();
			fg_Move(*this) > NPrivate::fg_DirectResultActor() / [pResult](TCAsyncResult<CReturnType> &&_Result)
				{
					pResult->m_Result = fg_Move(_Result);
					pResult->m_WaitEvent.f_SetSignaled();
				}
			;

			if (pResult->m_WaitEvent.f_WaitTimeout(_Timeout))
			{
				NStr::CStr CallstackStr = pResult->m_Result.f_GetExceptionCallstackStr(0);
				if (CallstackStr)
					DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to '{}' to finish. Call stack:\n{}", fg_GetTypeName<t_CFunctor>(), CallstackStr));
				else
					DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to '{}' to finish", fg_GetTypeName<t_CFunctor>()));
			}
			return pResult->m_Result.f_Move();
		}

		struct CNoUnwrapAsyncResult
		{
			TCActorCall *m_pWrapped;
			auto operator co_await() &&
			{
				return TCActorCallAwaiter<TCActorCall, CReturnType, false>{fg_Move(*m_pWrapped)};
			}
		};

		CNoUnwrapAsyncResult f_Wrap() &&
		{
			DMibFastCheck(!mp_Actor.f_IsEmpty());
			return {this};
		}

		auto operator co_await() &&
		{
			DMibFastCheck(!mp_Actor.f_IsEmpty());
			return TCActorCallAwaiter<TCActorCall, CReturnType, true>{fg_Move(*this)};
		}

		t_CActor mp_Actor;
		t_CFunctor mp_Functor;
		t_CParams mp_Params;

		static constexpr bool mc_bDirectCall = t_bDirectCall;
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
		struct TCCallMutipleActorStorage<t_bUnwrapTuple, t_CHandler, t_CActor, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>> : public NStorage::TCSharedPointerIntrusiveBase<>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<mint> m_nFinished;
			t_CActor m_Actor;
			t_CHandler m_Handler;

			TCCallMutipleActorStorage(t_CActor const &_Actor, t_CHandler &&_Handler)
				: m_Handler(fg_Move(_Handler))
				, m_Actor(_Actor)
			{
			}

			~TCCallMutipleActorStorage()
			{
				if (m_nFinished.f_Load() != mc_nResults)
					f_ReportResult();
			}

			void f_ReportResult()
			{
				static_assert(!NTraits::TCIsSame<t_CActor, TCActor<NPrivate::CDirectResultActor>>::mc_Value);
				NStorage::TCSharedPointer<TCCallMutipleActorStorage> pThis = fg_Explicit(this);
				m_Actor.f_GetRealActor()->f_QueueProcess
					(
						[pThis, Handler = fg_Move(m_Handler)]() mutable
						{
							auto &This = *pThis;
							auto &Internal = *This.m_Actor.f_GetRealActor()->fp_GetActor();
							CCurrentActorScope CurrentActor(&Internal);
							if constexpr (t_bUnwrapTuple)
							{
								Handler
									(
										fg_Move(fg_Get<tp_ResultIndices>(This.m_Results))...
									)
								;
							}
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
				bool t_bUnwrapTuple
				, typename t_CHandler
				, typename... tp_CResultTypes
				, mint... tp_ResultIndices
			>
		struct TCCallMutipleActorStorage<t_bUnwrapTuple, t_CHandler, TCActor<NPrivate::CDirectResultActor>, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>>
			: public NStorage::TCSharedPointerIntrusiveBase<>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};
			NStorage::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
			NAtomic::TCAtomic<mint> m_nFinished;
			TCActor<NPrivate::CDirectResultActor> m_Actor;
			t_CHandler m_Handler;

			TCCallMutipleActorStorage(TCActor<NPrivate::CDirectResultActor> const &_Actor, t_CHandler &&_Handler)
				: m_Handler(fg_Move(_Handler))
				, m_Actor(_Actor)
			{
			}

			~TCCallMutipleActorStorage()
			{
				if (m_nFinished.f_Load() != mc_nResults)
					f_ReportResult();
			}

			void f_ReportResult()
			{
				auto &This = *this;
				CCurrentActorScope CurrentActor(nullptr);
				if constexpr (t_bUnwrapTuple)
					m_Handler(fg_Move(fg_Get<tp_ResultIndices>(This.m_Results))...);
				else
					m_Handler(fg_Move(This.m_Results));
			}

			void f_Finished()
			{
				if (++m_nFinished == mc_nResults)
					f_ReportResult();
			}
		};

		template <typename t_CActorCall>
		struct TCGetActorCallFunctionPointer;

		template
		<
			typename t_CActor
			, typename t_CFunctionPtr
			, typename t_CParams
			, typename t_CTypeList
			, bool tf_bDirectCall
		>
		struct TCGetActorCallFunctionPointer<TCActorCall<t_CActor, t_CFunctionPtr, t_CParams, t_CTypeList, tf_bDirectCall>>
		{
			typedef t_CFunctionPtr CType;
		};
	}

	template <typename... tp_CCalls>
	template
	<
		typename tf_CResultActor
		, typename tf_CResultFunctor
		, mint... tfp_Indices
	>
	void TCActorCallPack<tp_CCalls...>::fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>) &&
	{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<typename tp_CCalls::CReturnType> && ...)>::mc_Value
					, "Incorrect types in result call"
				)
			;
#endif

		typedef NMeta::TCTypeList
			<
				typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType...
			> CTypeList
		;
		typedef NPrivate::TCCallMutipleActorStorage<true, tf_CResultFunctor, tf_CResultActor, CTypeList> CStorage;

#ifdef DMibContractConfigure_CheckEnabled
		auto Cleanup = g_OnScopeExit > [&]
			{
				fg_Swallow([this]{fg_Get<tfp_Indices>(m_Calls).mp_Actor.f_Clear(); return 0;}()...);
			}
		;
#endif

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(_ResultCall.mp_Actor, fg_Move(_ResultCall.mp_Functor));

		auto &Actor = NPrivate::fg_DirectResultActor();
		TCInitializerList<bool> Dummy =
			{
				NPrivate::fg_CallActorInternal
				(
					fg_Get<tfp_Indices>(m_Calls)
					, fg_TempCopy(Actor)
					, [pStorage](TCAsyncResult<typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType> &&_Result)
					{
						auto &Internal = *pStorage;
						fg_Get<tfp_Indices>(Internal.m_Results) = fg_Move(_Result);
						Internal.f_Finished();
					}
				)...
			}
		;
		(void)Dummy;
	}

#if DMibEnableSafeCheck > 0
	namespace NPrivate
	{
		mint fg_WrapActorCall(NFunction::TCFunctionNoAllocMovable<void ()> &&_fDoDisptach);
	}
#endif

	template
	<
		typename t_CActor
		, typename t_CRet
		, typename t_CFunctor
		, typename t_CResultActor
		, typename t_CResultFunctor
	>
	struct TCReportLocal
	{
		using CResultType = NConcurrency::TCAsyncResult<typename NPrivate::TCGetReturnType<t_CRet>::CType>;
		using CReturnType = typename NPrivate::TCGetReturnType<t_CRet>::CType;
		using CResultFunctorReturnType = typename NTraits::TCIsCallableWith<t_CResultFunctor, void (CResultType &&)>::CReturnType;
		using CResultFunctor = NFunction::TCFunctionMovable<void (CResultType &&_Result)>;

		struct CState
		{
			CState(t_CFunctor &&_ToCall, CResultFunctor &&_ResultFunctor, TCActor<t_CResultActor> const &_ResultActor)
				: m_ToCall(fg_Move(_ToCall))
				, m_ResultFunctor(fg_Move(_ResultFunctor))
				, m_ResultActor(_ResultActor)
			{
			}

			t_CFunctor m_ToCall;
			CResultFunctor m_ResultFunctor;
			TCActor<t_CResultActor> m_ResultActor;
#if DMibConfig_Concurrency_DebugActorCallstacks
			CAsyncCallstacks m_Callstacks;
#endif
		};

		TCReportLocal(TCReportLocal &&_Other)
			: m_pState(fg_Move(_Other.m_pState))
			, m_pActorInternal(_Other.m_pActorInternal)
		{
			_Other.m_pActorInternal = nullptr;
		}

		TCReportLocal
			(
				t_CFunctor &&_ToCall
				, t_CResultFunctor &&_ResultFunctor
				, TCActor<t_CResultActor> const &_ResultActor
				, TCActorInternal<t_CActor> *_pActorInternal
			)
			: m_pState(fg_Construct(fg_Move(_ToCall), fg_Move(_ResultFunctor), _ResultActor))
			, m_pActorInternal(_pActorInternal)
		{
#if DMibConfig_Concurrency_DebugActorCallstacks
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (ThreadLocal.m_pCallstacks)
				m_pState->m_Callstacks = *ThreadLocal.m_pCallstacks;

			auto &Callstack = m_pState->m_Callstacks.f_InsertFirst();
			Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(Callstack.m_Callstack, sizeof(Callstack.m_Callstack) / sizeof(Callstack.m_Callstack[0]));
#	ifdef DMibDebug
			mint nFramesToRemove = 4; // fg_System_GetStackTrace + Constructor + f_Call + operator >
#		ifdef DCompiler_clang
			nFramesToRemove += 1; // Two frames for constructor
#		endif
#	else
			mint nFramesToRemove = 0; // We don't know what inlining will do
#	endif
			NMemory::fg_MemMove(Callstack.m_Callstack, Callstack.m_Callstack + nFramesToRemove, sizeof(Callstack.m_Callstack) - sizeof(Callstack.m_Callstack[0]) * nFramesToRemove);
			Callstack.m_CallstackLen -= nFramesToRemove;

			if (m_pState->m_Callstacks.f_GetLen() > 16)
				m_pState->m_Callstacks.f_Remove(m_pState->m_Callstacks.f_GetLast());
#endif
		}

		inline_always void f_ResultAvailable(CResultType &&_Result)
		{
			auto &State = *m_pState;
			if constexpr (mc_ShouldCallResultDirect)
			{
				m_pActorInternal = nullptr;

				NPrivate::fg_CallResultFunctorDirect(State.m_ResultFunctor, fg_Move(_Result));
			}
			else if constexpr (mc_ShouldDiscardResults)
				m_pActorInternal = nullptr;
			else
			{
				static_assert(!NTraits::TCIsSame<t_CResultActor, NPrivate::CDirectResultActor>::mc_Value);
				auto pActor = State.m_ResultActor.f_GetRealActor();
				m_pActorInternal = nullptr;
				pActor->f_QueueProcess
					(
					 	[pState = fg_Move(m_pState), Result = fg_Move(_Result)]() mutable
					 	{
							NPrivate::fg_CallResultFunctor(pState->m_ResultFunctor, pState->m_ResultActor.f_GetRealActor()->fp_GetActor(), fg_Move(Result));
						}
					)
				;
			}
		}

#if DMibEnableSafeCheck > 0
		inline_never
#endif
		void operator ()()
		{
			DMibFastCheck(m_pActorInternal);

			if constexpr (NPrivate::TCIsFuture<typename t_CFunctor::CReturnType>::mc_Value)
			{
				auto pThis = this;
#if DMibEnableSafeCheck > 0
				NPrivate::fg_WrapActorCall
				(
				 	[pThis]() mutable
				 	{
#endif

						auto &State = *pThis->m_pState;
#if DMibConfig_Concurrency_DebugActorCallstacks
						auto &Callstack = State.m_Callstacks;
						NPrivate::CAsyncCallstacksScope CallstacksScope(Callstack);
#endif

#if DMibEnableSafeCheck > 0
						auto &ThreadLocal = **g_SystemThreadLocal;
						bool bPreviousExpectCoroutineCall = ThreadLocal.m_bExpectCoroutineCall;
						ThreadLocal.m_bExpectCoroutineCall = true;
						auto Cleanup = g_OnScopeExit > [&]
							{
								ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
							}
						;
#endif

						auto pActor = pThis->m_pActorInternal->fp_GetActor();
						CCurrentActorScope CurrentActor(pActor);

#if DMibEnableSafeCheck > 0
						TCFuture<CReturnType> Future;
						try
						{
							Future = State.m_ToCall(*pActor, typename t_CFunctor::CIndices());
						}
						catch (NException::CDebugException const &)
						{
							Future = NException::fg_CurrentException();
						}
#else
						TCFuture<CReturnType> Future = State.m_ToCall(*pActor, typename t_CFunctor::CIndices());
#endif

#if DMibEnableSafeCheck > 0
						ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
						Cleanup.f_Clear();
#endif

						if constexpr (mc_ShouldDiscardResults)
						{
							if (!Future.f_IsCoroutine())
							{
								Future.f_DiscardResult();
								return;
							}
							// Coroutine param state needs to be kept alive until coroutine has resloved
						}

						Future.f_OnResultSet
							(
								[Local = fg_Move(*pThis)](auto &&_Result) mutable
								{
									if constexpr (!mc_ShouldDiscardResults)
									{
										if (_Result.f_IsSet())
										{
#if DMibConfig_Concurrency_DebugActorCallstacks
											_Result.m_Callstacks = fg_Move(Local.m_pState->m_Callstacks);
#endif
										}
										else
											_Result.f_SetException(DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "Result was not set", false));
									}

									Local.f_ResultAvailable(fg_Move(_Result));
								}
							)
						;

#if DMibEnableSafeCheck > 0
				 	}
				);
#endif
			}
			else
			{
				auto pActor = m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(pActor);
				CResultType Result;
				{
					auto &State = *m_pState;
#if DMibConfig_Concurrency_DebugActorCallstacks
					auto &Callstack = State.m_Callstacks;
					NPrivate::CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					if constexpr (mc_ShouldDiscardResults)
						State.m_ToCall(*(pActor), typename t_CFunctor::CIndices());
					else
					{
						if constexpr (NTraits::TCIsSame<CResultType, TCAsyncResult<void>>::mc_Value)
						{
							State.m_ToCall(*pActor, typename t_CFunctor::CIndices());
							Result.f_SetResult();
						}
						else
							Result.f_SetResult(State.m_ToCall(*(pActor), typename t_CFunctor::CIndices()));
					}
				}
				(*this).f_ResultAvailable(fg_Move(Result));
			}
		}

		inline_always CConcurrencyManager &f_ConcurrencyManager()
		{
			if constexpr (NTraits::TCIsSame<t_CActor, CAnyConcurrentActor>::mc_Value || NTraits::TCIsSame<t_CActor, CAnyConcurrentActorLowPrio>::mc_Value)
				return fg_ConcurrencyManager();
			return m_pActorInternal->f_ConcurrencyManager();
		}

		~TCReportLocal()
		{
			if constexpr (!mc_ShouldDiscardResults)
			{
				if (m_pActorInternal)
				{
					CResultType Result;
#if DMibConfig_Concurrency_DebugBlockDestroy
					Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, fg_Format("Actor '{}' called has been deleted", m_pActorInternal->m_ActorTypeName)));
#else
					Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted", false));
#endif
					f_ResultAvailable(fg_Move(Result));
				}
			}
		}

		static_assert(NTraits::TCIsSame<CResultFunctorReturnType, void>::mc_Value, "Result handlers cannot return");
#if 0
		// Coroutines in result handlers are not allowed as any errors will go unobserved. Instead do this:

		fg_ActorCall() > [=]
			{
				self / [=]() -> TCContinutaion<void>
					{
						co_await fg_AsyncCall();
					}
					> [=](TCAsyncResult<void> &&_Error)
					{
						// Handle error here
					}
				;
			}
		;
#endif

		constexpr static const bool mc_ShouldCallResultDirect = NTraits::TCIsSame<t_CResultActor, NPrivate::CDirectResultActor>::mc_Value;
		constexpr static const bool mc_ShouldDiscardResults = NTraits::TCIsSame<t_CResultFunctor, NPrivate::CDiscardResultFunctor>::mc_Value;

		NStorage::TCUniquePointer<CState> m_pState;
		TCActorInternal<t_CActor> *m_pActorInternal;

	private:
		TCReportLocal(TCReportLocal const &_Other);
	};
}

#include "Malterlib_Concurrency_ActorCallCoroutine.hpp"
