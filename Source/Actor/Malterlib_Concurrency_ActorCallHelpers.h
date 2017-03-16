// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

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
				>
				(
					Actor
					, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
					, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
				)
			;
		}
		
		template <typename t_CMemberPointer, typename t_CTuple, typename t_CIndices, typename t_COriginalParams>
		struct TCActorCallWithParams;
		
		template <typename tf_CPromoteFrom, typename tf_CParam>
		auto fg_Promote(tf_CParam &&_Param) -> typename NTraits::TCPromoteQualifiersAndReference<tf_CPromoteFrom, tf_CParam>::CType
		{
			return fg_Forward<typename NTraits::TCPromoteQualifiersAndReference<tf_CPromoteFrom, tf_CParam>::CType>(_Param);
		}

		template <typename t_CMemberPointer, typename... tp_CTupleParams, mint... tp_Indices, typename... tp_COriginalParams>
		struct TCActorCallWithParams<t_CMemberPointer, DMibTupleTemplate<tp_CTupleParams...>, NMeta::TCIndices<tp_Indices...>, NMeta::TCTypeList<tp_COriginalParams...>>
		{
			t_CMemberPointer m_pMemberPointer;

			NContainer::TCTuple<tp_CTupleParams...> m_Params;

			template <typename... tfp_CParams>
			TCActorCallWithParams(t_CMemberPointer _pMemberPointer, tfp_CParams &&...p_Params)
				: m_pMemberPointer(_pMemberPointer)
				, m_Params(fg_Forward<tfp_CParams>(p_Params)...)
			{
			}
			TCActorCallWithParams(TCActorCallWithParams &&_Other)
				: m_pMemberPointer(fg_Move(_Other.m_pMemberPointer))
				, m_Params(fg_Move(_Other.m_Params))
			{
			}

			template <typename tf_CActor>
			typename NTraits::TCMemberFunctionPointerTraits<t_CMemberPointer>::CReturn operator() (tf_CActor &_Actor)
			{
				return (_Actor.*m_pMemberPointer)(fg_Promote<tp_COriginalParams>(NContainer::fg_Get<tp_Indices>(m_Params))...);
			}
		private:
			TCActorCallWithParams(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams const&);
			TCActorCallWithParams &operator =(TCActorCallWithParams &&);
		};

		template <typename tf_CToBind, typename... tfp_CParams, mint... tfp_Indicies, typename tf_CRawParams>
		auto fg_BindTupleHelper(tf_CToBind &&_ToBind, NContainer::TCTuple<tfp_CParams...> const &_Params, NMeta::TCIndices<tfp_Indicies...>, tf_CRawParams)
		{
			return TCActorCallWithParams
				<
					typename NTraits::TCRemoveReference<tf_CToBind>::CType
					, decltype(NContainer::fg_Tuple(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indicies>(fg_RemoveQualifiers(_Params)))...))
					, NMeta::TCIndices<tfp_Indicies...>
					, tf_CRawParams
				>
				(
					fg_Forward<tf_CToBind>(_ToBind)
					, fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indicies>(fg_RemoveQualifiers(_Params)))...
				)
			;
		}
		
		template <typename tf_CTypeList, typename tf_CToBind, typename... tfp_CParams>
		auto fg_BindHelper(tf_CToBind &&_ToBind, NContainer::TCTuple<tfp_CParams...> const &_Params)
		{
			return fg_BindTupleHelper
				(
					fg_Forward<tf_CToBind>(_ToBind)
					, _Params
					, typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType()
					, tf_CTypeList()
				)
			;
		}
		
		template <typename tf_CToBind, typename... tfp_CParams, mint... tfp_Indicies>
		auto fg_BindConstructHelper(tf_CToBind &&_ToBind, TCConstruct<void, tfp_CParams...> const &_Params, NMeta::TCIndices<tfp_Indicies...>)
		{
			return TCActorCallWithParams
				<
					typename NTraits::TCRemoveReference<tf_CToBind>::CType
					, decltype(NContainer::fg_Tuple(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indicies>(fg_RemoveQualifiers(_Params.m_Params)))...))
					, NMeta::TCIndices<tfp_Indicies...>
					, NMeta::TCTypeList<tfp_CParams...>
				>
				(
					fg_Forward<tf_CToBind>(_ToBind)
					, fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indicies>(fg_RemoveQualifiers(_Params.m_Params)))...
				)
			;
		}
		
		template <typename tf_CTypeList, typename tf_CToBind, typename... tfp_CParams>
		auto fg_BindHelper(tf_CToBind &&_ToBind, TCConstruct<void, tfp_CParams...> const &_Params)
		{
			return fg_BindConstructHelper
				(
					fg_Forward<tf_CToBind>(_ToBind)
					, _Params
					, typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType()
				)
			;
		}
		
		template <typename... tfp_CParams>
		auto fg_ToTupleByValue(NContainer::TCTuple<tfp_CParams...> &&_Params)
		{
			return fg_Move(_Params);
		}
		
		template <typename... tfp_CParams, mint... tfp_Indicies>
		auto fg_ToTupleByValueHelper(TCConstruct<void, tfp_CParams...> &&_Params, NMeta::TCIndices<tfp_Indicies...>)
		{
			return NContainer::fg_Tuple
				(
					NContainer::fg_Get<tfp_Indicies>(fg_Move(_Params.m_Params))...
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
								NContainer::fg_Get<tfp_Indices>(Result) = fg_Move(*p_Results);
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
	}
	
	template <typename... tp_CCalls>
	struct TCActorCallPack
	{
		NContainer::TCTuple<tp_CCalls...> m_Calls;
		
		TCActorCallPack(NContainer::TCTuple<tp_CCalls...> &&_Tuple)
			: m_Calls(fg_Move(_Tuple))
		{
		}
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		TCActorCallPack<tp_CCalls..., TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList>> 
		operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_OtherCall)
		{
			return NContainer::fg_TupleConcatenate(fg_Move(m_Calls), NContainer::fg_Tuple(fg_Move(_OtherCall)));
		}

		template <typename tf_CType>
		auto operator + (TCContinuation<tf_CType> const &_Continuation);
		
		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall)
		{
			fp_ActorCall(fg_Move(_ResultCall), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());
		}

		template <typename tf_CFunctor, TCEnableIfType<!TCIsActorResultCall<tf_CFunctor>::mc_Value> * = nullptr>
		void operator > (tf_CFunctor &&_Functor)
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			fp_ActorCall(pActor / fg_Forward<tf_CFunctor>(_Functor), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());				
		}
		
		auto f_CallSync(fp64 _Timeout = -1.0)
		{
			return fp_CallSync(_Timeout);
		}
		
	private:
		NContainer::TCTuple<typename tp_CCalls::CReturnType...> fp_CallSync(fp64 _Timeout)
		{
			using CReturnType = NContainer::TCTuple<typename tp_CCalls::CReturnType...>;
			NPtr::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType>> pResult = fg_Construct();
			*this > NPrivate::fg_DirectResultActor() / [pResult](auto &&...p_Results)
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
		void fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>);
	};
	
	namespace NPrivate
	{
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultActor, typename tf_CResultFunctor>
		bool fg_CallActorInternal
			(
				TCActorCall<TCActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList> &_ActorCall
				, TCActor<tf_CResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		{
			_ActorCall.mp_Actor->template f_Call<tf_CFunctor>
				(
					NPrivate::fg_BindHelper<tf_CTypeList>
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

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultFunctor>
		bool fg_CallActorInternal
			(
				TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList> &_ActorCall
				, TCActor<NPrivate::CDirectResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		{
#if DMibEnableSafeCheck > 0
			using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>;
#endif
			
			auto Actor = _ActorCall.mp_Actor.f_Lock();
			if (!Actor)
			{
				TCAsyncResult<typename TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList>::CReturnType> Result;
				Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted"));
				NPrivate::fg_CallResultFunctorDirect(_fResultFunctor, fg_Move(Result));
				return true;
			}

			// If you get this, use DCallActor instead of calling directly
			DMibFastCheck(Actor.f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value)  || !Actor->f_GetDistributedActorData() || Actor->f_GetDistributedActorData()->f_IsValidForCall());
			
			Actor->template f_Call<tf_CFunctor>
				(
					NPrivate::fg_BindHelper<tf_CTypeList>
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
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultActor, typename tf_CResultFunctor>
		bool fg_CallActorInternal
			(
				TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList> &_ActorCall
				, TCActor<tf_CResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		{
#if DMibEnableSafeCheck > 0
			using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<tf_CFunctor>;
#endif
			
			auto Actor = _ActorCall.mp_Actor.f_Lock();
			if (!Actor)
			{
				if (NTraits::TCIsSame<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CResultFunctor>::CType, NPrivate::CDiscardResultFunctor>::mc_Value)
					return true;

				static_assert(!NTraits::TCIsSame<tf_CResultActor, NPrivate::CDirectResultActor>::mc_Value, "");
				
				auto ResultActor = fg_GetResultActor(_ResultActor);
				ResultActor->f_QueueProcess
					(
						[ResultActor, fResultFunctor = fg_Move(_fResultFunctor)]() mutable
						{
							TCAsyncResult<typename TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList>::CReturnType> Result;
							Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Weak actor called has been deleted"));
							NPrivate::fg_CallResultFunctor(fResultFunctor, ResultActor->fp_GetActor(), fg_Move(Result));
						}
					)
				;
				return true;
			}

			// If you get this, use DCallActor instead of calling directly
			DMibFastCheck(Actor.f_IsEmpty() || (NTraits::TCIsSame<typename CMemberPointerTraits::CClass, CActor>::mc_Value)  || !Actor->f_GetDistributedActorData() || Actor->f_GetDistributedActorData()->f_IsValidForCall());
			
			Actor->template f_Call<tf_CFunctor>
				(
					NPrivate::fg_BindHelper<tf_CTypeList>
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

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
	struct TCActorCall
	{
		static_assert(!NTraits::TCIsReference<t_CActor>::mc_Value, "Incorrect type");
		static_assert(!NTraits::TCIsReference<t_CFunctor>::mc_Value, "Incorrect type");
		static_assert(!NTraits::TCIsReference<t_CParams>::mc_Value, "Incorrect type");
		
		using CReturnType = typename NPrivate::TCGetReturnType<typename NTraits::TCMemberFunctionPointerTraits<t_CFunctor>::CReturn>::CType;
		using CTypeList = t_CTypeList;
		using CFunctor = t_CFunctor;
		
		t_CActor mp_Actor;
		t_CFunctor mp_Functor;
		t_CParams mp_Params;
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

		TCActorCall(TCActorCall const &_Other)
			: mp_Actor(_Other.mp_Actor)
			, mp_Functor(_Other.mp_Functor)
			, mp_Params(_Other.mp_Params)
		{
		}

		TCActorCall(TCActorCall &&_Other)
			: mp_Actor(fg_Move(_Other.mp_Actor))
			, mp_Functor(fg_Move(_Other.mp_Functor))
			, mp_Params(fg_Move(_Other.mp_Params))
		{
		}

#ifdef DMibContractConfigure_CheckEnabled			
		~TCActorCall()
		{
			DMibCheck(mp_Actor.f_IsEmpty())("Actor call without result used");
		}
#endif

		auto f_ByValue()
		{
			return TCActorCall<t_CActor, t_CFunctor, decltype(NPrivate::fg_ToTupleByValue(fg_Move(mp_Params))), t_CTypeList>
				{
					fg_Move(mp_Actor)
					, fg_Move(mp_Functor)
					, NPrivate::fg_ToTupleByValue(fg_Move(mp_Params))
				}
			;
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
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_OtherCall)
			-> TCActorCallPack<TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList>, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList>> 
		{
			return TCActorCallPack<TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList>, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList>>
				(NContainer::fg_Tuple(fg_Move(*this), fg_Move(_OtherCall)))
			;
		}

		template <typename tf_CType>
		auto operator + (TCContinuation<tf_CType> const &_Continuation);

		template 
		<
			typename tf_CFunctor
			, TCEnableIfType
			<
				!TCIsActorResultCall<tf_CFunctor>::mc_Value 
				&& !NPrivate::TCIsContinuation<typename NTraits::TCRemoveReference<tf_CFunctor>::CType>::mc_Value
				&& !NPrivate::TCIsContinuationWithError<typename NTraits::TCRemoveReference<tf_CFunctor>::CType>::mc_Value
			> * = nullptr
		>
		void operator > (tf_CFunctor &&_Functor)
		{
			auto pActor = fg_CurrentActor();
			DMibFastCheck(pActor);
			*this > pActor / fg_Forward<tf_CFunctor>(_Functor);
		}
		
		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall)
		{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<CReturnType> &&)>::mc_Value
					, "Incorrect type in result call"
				)
			;
#endif
			DMibFastCheck(!mp_Actor.f_IsEmpty() || t_CActor::CContainedActor::mc_bCanBeEmpty);
			NPrivate::fg_CallActorInternal(*this, fg_Move(_ResultCall.mp_Actor), _ResultCall.mp_Functor);
#ifdef DMibContractConfigure_CheckEnabled
			mp_Actor.f_Clear();
#endif
		}

		template <typename tf_CResult>
		void operator > (TCContinuation<tf_CResult> const &_Continuation)
		{
			*this > NPrivate::fg_DirectResultActor() / [_Continuation](TCAsyncResult<tf_CResult> &&_Result)
				{
					_Continuation.f_SetResult(fg_Move(_Result));
				}
			;
		}
		
		template <typename tf_CResult>
		void operator > (TCContinuationWithError<tf_CResult> const &_ContinuationWithError)
		{
			*this > NPrivate::fg_DirectResultActor() / 
				(
					_ContinuationWithError / [Continuation = _ContinuationWithError.m_Continuation](tf_CResult &&_Result)
					{
						Continuation.f_SetResult(fg_Move(_Result));
					}
				)
			;
		}

		void operator > (TCContinuationWithError<void> const &_ContinuationWithError)
		{
			*this > NPrivate::fg_DirectResultActor() / 
				(
					_ContinuationWithError / [Continuation = _ContinuationWithError.m_Continuation]()
					{
						Continuation.f_SetResult();
					}
				)
			;
		}
		
		TCDispatchedActorCall<CReturnType> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage); // #include <Mib/Concurrency/Actor/Timer> to use
		
		auto f_CallSync()
		{
			TCAsyncResult<CReturnType> Result;
			NThread::CEvent WaitEvent;
			*this > NPrivate::fg_DirectResultActor() / [&](TCAsyncResult<CReturnType> &&_Result)
				{
					Result = fg_Move(_Result);
					WaitEvent.f_SetSignaled();
				}
			;

			WaitEvent.f_Wait();

			return Result.f_Move();
		}

		auto f_CallSync(fp64 _Timeout)
		{
			NPtr::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType>> pResult = fg_Construct();
			*this > NPrivate::fg_DirectResultActor() / [pResult](TCAsyncResult<CReturnType> &&_Result)
				{
					pResult->m_Result = fg_Move(_Result);
					pResult->m_WaitEvent.f_SetSignaled();
				}
			;

			if (pResult->m_WaitEvent.f_WaitTimeout(_Timeout))
				DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to '{}' to finish", fg_GetTypeName<t_CFunctor>()));

			return pResult->m_Result.f_Move();
		}
	};

	namespace NPrivate
	{

		template <typename t_CType, typename t_CStrippedType = typename NTraits::TCMemberFunctionPointerTraits<t_CType>::CReturn>
		struct TCGetResultType
		{
			typedef t_CStrippedType CType;
		};
		template <typename t_CType, typename t_CContinuationType>
		struct TCGetResultType<t_CType, TCContinuation<t_CContinuationType>>
		{
			typedef t_CContinuationType CType;
		};
		
		template 
			<
				typename t_CHandler
				, typename t_CActor
				, typename... tp_CResultTypes
				, mint... tp_ResultIndices
			>
		struct TCCallMutipleActorStorage<t_CHandler, t_CActor, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>> : public NPtr::TCSharedPointerIntrusiveBase<>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};
			NContainer::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
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
				static_assert(!NTraits::TCIsSame<t_CActor, TCActor<NPrivate::CDirectResultActor>>::mc_Value, "");
				NPtr::TCSharedPointer<TCCallMutipleActorStorage> pThis = fg_Explicit(this);
				m_Actor.f_GetActor()->f_QueueProcess
					(
						[pThis, Handler = fg_Move(m_Handler)]() mutable
						{
							auto &This = *pThis;
							auto &Internal = *This.m_Actor.f_GetActor()->fp_GetActor();
							CCurrentActorScope CurrentActor(&Internal);
							Handler
								(
									fg_Move(NContainer::fg_Get<tp_ResultIndices>(This.m_Results))...
								)
							;
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
				typename t_CHandler
				, typename... tp_CResultTypes
				, mint... tp_ResultIndices
			>
		struct TCCallMutipleActorStorage<t_CHandler, TCActor<NPrivate::CDirectResultActor>, NMeta::TCTypeList<tp_CResultTypes...>, NMeta::TCIndices<tp_ResultIndices...>> : public NPtr::TCSharedPointerIntrusiveBase<>
		{
			enum
			{
				mc_nResults = sizeof...(tp_CResultTypes)
			};
			NContainer::TCTuple<TCAsyncResult<tp_CResultTypes>...> m_Results;
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
				auto Handler = fg_Move(m_Handler);
				Handler
					(
						fg_Move(NContainer::fg_Get<tp_ResultIndices>(This.m_Results))...
					)
				;
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
		>
		struct TCGetActorCallFunctionPointer<TCActorCall<t_CActor, t_CFunctionPtr, t_CParams, t_CTypeList>>
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
	void TCActorCallPack<tp_CCalls...>::fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>)
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
		typedef NPrivate::TCCallMutipleActorStorage<tf_CResultFunctor, tf_CResultActor, CTypeList> CStorage;

		NPtr::TCSharedPointer<CStorage> pStorage = fg_Construct(_ResultCall.mp_Actor, fg_Move(_ResultCall.mp_Functor));
		
		auto &Actor = NPrivate::fg_DirectResultActor();
		TCInitializerList<bool> Dummy =
			{
				NPrivate::fg_CallActorInternal
				(
					NContainer::fg_Get<tfp_Indices>(m_Calls)
					, fg_TempCopy(Actor)
					, [pStorage](TCAsyncResult<typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType> &&_Result)
					{
						auto &Internal = *pStorage;
						NContainer::fg_Get<tfp_Indices>(Internal.m_Results) = fg_Move(_Result);
						Internal.f_Finished();
					}
				)...
			}
		;
		(void)Dummy;
#ifdef DMibContractConfigure_CheckEnabled
		fg_Swallow([this]{NContainer::fg_Get<tfp_Indices>(m_Calls).mp_Actor.f_Clear(); return 0;}()...);
#endif
	}	

	template 
	<
		typename tf_CResultActor
		, TCEnableIfType
		<
			!NTraits::TCIsSame<tf_CResultActor, CAnyConcurrentActor>::mc_Value
			&& !NTraits::TCIsSame<tf_CResultActor, CAnyConcurrentActorLowPrio>::mc_Value
		> * = nullptr
	>
	inline_always TCActor<tf_CResultActor> const &fg_GetResultActor(TCActor<tf_CResultActor> const &_ResultActor)
	{
		static_assert(!NTraits::TCIsSame<tf_CResultActor, NPrivate::CDirectResultActor>::mc_Value, "");
		return _ResultActor;
	}
	
	template 
	<
		typename tf_CResultActor
		, TCEnableIfType
		<
			NTraits::TCIsSame<tf_CResultActor, CAnyConcurrentActor>::mc_Value
			|| NTraits::TCIsSame<tf_CResultActor, CAnyConcurrentActorLowPrio>::mc_Value
		> * = nullptr
	>
	inline_always TCActor<CConcurrentActor> const &fg_GetResultActor(TCActor<tf_CResultActor> const &_ResultActor)
	{
		return fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(NTraits::TCIsSame<tf_CResultActor, CAnyConcurrentActorLowPrio>::mc_Value ? EPriority_Low : EPriority_Normal);
	}
	
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
		mutable t_CFunctor m_ToCall;
		mutable NConcurrency::TCAsyncResult<typename NPrivate::TCGetReturnType<t_CRet>::CType> m_Result;
		
		using CResultFunctor = NFunction::TCFunctionMovable<void (NConcurrency::TCAsyncResult<typename NPrivate::TCGetReturnType<t_CRet>::CType> &&_Result)>;
		
		mutable CResultFunctor m_ResultFunctor;
		mutable TCActor<t_CResultActor> m_pResultActor;
		mutable TCActorInternal<t_CActor> *m_pActorInternal;

		TCReportLocal(TCReportLocal &&_Other)
			: m_ToCall(fg_Move(_Other.m_ToCall))
			, m_Result(fg_Move(_Other.m_Result))
			, m_ResultFunctor(fg_Move(_Other.m_ResultFunctor))
			, m_pResultActor(fg_Move(_Other.m_pResultActor))
			, m_pActorInternal(_Other.m_pActorInternal)
		{
			_Other.m_pActorInternal = nullptr;
		}
		
		TCReportLocal
			(
				t_CFunctor &&_ToCall
				, t_CResultFunctor &&_ResultFunctor
				, TCActor<t_CResultActor> const &_pResultActor
				, TCActorInternal<t_CActor> *_pActorInternal
			)
			: m_ToCall(fg_Move(_ToCall))
			, m_ResultFunctor(fg_Move(_ResultFunctor))
			, m_pResultActor(_pResultActor)
			, m_pActorInternal(_pActorInternal)
		{
#if DMibConcurrencyDebugActorCallstacks
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (ThreadLocal.m_pCallstacks)
				m_Result.m_Callstacks = *ThreadLocal.m_pCallstacks;
			
			auto &Callstack = m_Result.m_Callstacks.f_InsertFirst();
			Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(Callstack.m_Callstack, sizeof(Callstack.m_Callstack) / sizeof(Callstack.m_Callstack[0]));
#	ifdef DMibDebug
			mint nFramesToRemove = 4; // fg_System_GetStackTrace + Constructor + f_Call + operator >
#		ifdef DCompiler_clang
			nFramesToRemove += 1; // Two frames for constructor
#		endif
#	else
			mint nFramesToRemove = 0; // We don't know what inlining will do
#	endif
			NMem::fg_MemMove(Callstack.m_Callstack, Callstack.m_Callstack + nFramesToRemove, sizeof(Callstack.m_Callstack) - sizeof(Callstack.m_Callstack[0]) * nFramesToRemove);
			Callstack.m_CallstackLen -= nFramesToRemove;
			
			if (m_Result.m_Callstacks.f_GetLen() > 16)
				m_Result.m_Callstacks.f_Remove(m_Result.m_Callstacks.f_GetLast());
#endif				
		}
		
		template <bool t_bCallDirect, TCEnableIfType<t_bCallDirect> * = nullptr>
		inline_always void fp_CallResult() const
		{
			NPrivate::fg_CallResultFunctorDirect(m_ResultFunctor, fg_Move(m_Result));
		}
		
		template <bool t_bCallDirect, TCEnableIfType<!t_bCallDirect> * = nullptr>
		inline_always void fp_CallResult() const
		{
			NPrivate::fg_CallResultFunctor(m_ResultFunctor, fg_GetResultActor(m_pResultActor)->fp_GetActor(), fg_Move(m_Result));
		}

		constexpr static inline_always bool fs_ShouldCallResultDirect()
		{
			return NTraits::TCIsSame<t_CResultActor, NPrivate::CDirectResultActor>::mc_Value;
		}
		
		constexpr static inline_always bool fs_ShouldDiscardResult()
		{
			return NTraits::TCIsSame<t_CResultFunctor, NPrivate::CDiscardResultFunctor>::mc_Value;
		}
		
		template <bool t_bCallDirect = fs_ShouldCallResultDirect(), bool t_bDiscard = fs_ShouldDiscardResult(), TCEnableIfType<t_bCallDirect> * = nullptr>
		inline_always void f_ResultAvailable()
		{
			m_pActorInternal = nullptr;
			(*this)();
		}

		template <bool t_bCallDirect = fs_ShouldCallResultDirect(), bool t_bDiscard = fs_ShouldDiscardResult(), TCEnableIfType<t_bDiscard> * = nullptr>
		inline_always void f_ResultAvailable()
		{
			m_pActorInternal = nullptr;
		}
		
		template <bool t_bCallDirect = fs_ShouldCallResultDirect(), bool t_bDiscard = fs_ShouldDiscardResult(), TCEnableIfType<!t_bCallDirect && !t_bDiscard> * = nullptr>
		inline_always void f_ResultAvailable()
		{
			auto pActor = fg_GetResultActor(m_pResultActor);
			m_pActorInternal = nullptr;
			pActor->f_QueueProcess(fg_Move(*this));
		}
		
		void operator ()() const
		{
			if (m_Result.f_IsSet())
				fp_CallResult<fs_ShouldCallResultDirect()>();
			else
			{
				NPrivate::fg_CallWithAsyncResult
					<
						decltype(m_Result)
						, decltype(m_ToCall)
						, decltype(*(m_pActorInternal->fp_GetActor()))
					>
					(*this)
				;
			}
		}
		
		inline_always CConcurrencyManager &f_ConcurrencyManager()
		{
			if (NTraits::TCIsSame<t_CActor, CAnyConcurrentActor>::mc_Value || NTraits::TCIsSame<t_CActor, CAnyConcurrentActorLowPrio>::mc_Value)
				return fg_ConcurrencyManager();
			return m_pActorInternal->f_ConcurrencyManager();
		}
		
		template <bool tf_bDiscard, TCEnableIfType<tf_bDiscard> * = nullptr>
		void fp_Destruct()
		{
		}
		
		template <bool tf_bDiscard, TCEnableIfType<!tf_bDiscard> * = nullptr>
		void fp_Destruct()
		{
			if (m_pActorInternal)
			{
#ifdef DMibDebug
				m_Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, fg_Format("Actor '{}' called has been deleted", m_pActorInternal->m_ActorTypeName)));
#else
				m_Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted"));
#endif
				f_ResultAvailable<>();
			}
		}
		
		~TCReportLocal()
		{
			fp_Destruct<fs_ShouldDiscardResult()>();
		}
	private:
		TCReportLocal(TCReportLocal const &_Other);
	};

}
