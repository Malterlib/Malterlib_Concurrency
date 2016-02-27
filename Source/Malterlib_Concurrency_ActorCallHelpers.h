// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			CCurrentActorScope::CCurrentActorScope(NConcurrency::CConcurrencyManager &_ConcurrencyManager, CActor const *_pActor)
				: mp_ConcurrencyManager(_ConcurrencyManager)
			{
				auto &ThreadLocal = *_ConcurrencyManager.m_ThreadLocal;
				mp_pLastActor = ThreadLocal.m_pCurrentActor;
				ThreadLocal.m_pCurrentActor = const_cast<CActor *>(_pActor);
				
			}
			CCurrentActorScope::~CCurrentActorScope()
			{
				auto &ThreadLocal = *mp_ConcurrencyManager.m_ThreadLocal;
				ThreadLocal.m_pCurrentActor = mp_pLastActor;
			}
			
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto CThisActor::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
			{
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

			template <typename tf_CToBind, typename... tfp_CParams, mint... tfp_Indicies>
			auto fg_BindConstructHelper(tf_CToBind &&_ToBind, TCConstruct<void, tfp_CParams...> const &_Params, NMeta::TCIndices<tfp_Indicies...> )
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
			
			template <typename tf_CToBind, typename... tfp_CParams>
			auto fg_BindConstruct(tf_CToBind &&_ToBind, TCConstruct<void, tfp_CParams...> const &_Params)
			{
				return fg_BindConstructHelper
					(
						fg_Forward<tf_CToBind>(_ToBind)
						, _Params
						, typename NMeta::TCMakeConsecutiveIndices<TCConstruct<void, tfp_CParams...>::mc_nParams>::CType()
					)
				;
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

		template <typename... tp_CCalls>
		struct TCActorCallPack
		{
			NContainer::TCTuple<tp_CCalls...> m_Calls;
			
			TCActorCallPack(NContainer::TCTuple<tp_CCalls...> &&_Tuple)
				: m_Calls(fg_Move(_Tuple))
			{
			}
			
			template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams>
			TCActorCallPack<tp_CCalls..., TCActorCall<tf_CActor, tf_CFunctor, tf_CParams>> 
			operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams> &&_OtherCall)
			{
				return NContainer::fg_TupleConcatenate(fg_Move(m_Calls), NContainer::fg_Tuple(fg_Move(_OtherCall)));
			}

			template <typename tf_CResultActor, typename tf_CResultFunctor>
			void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall)
			{
				fp_ActorCall(fg_Move(_ResultCall), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());
			}

			template <typename tf_CFunctor, TCEnableIfType<!TCIsActorResultCall<tf_CFunctor>::mc_Value> * = nullptr>
			void operator > (tf_CFunctor &&_Functor)
			{
				auto pActor = NContainer::fg_Get<0>(m_Calls).mp_Actor->f_ConcurrencyManager().m_ThreadLocal->m_pCurrentActor;
				DMibFastCheck(pActor);
				fp_ActorCall(fg_ThisActor(pActor) / fg_Forward<tf_CFunctor>(_Functor), typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());				
			}
			
		private:
			template <typename tf_CResultActor, typename tf_CResultFunctor, mint... tfp_Indices>
			void fp_ActorCall(TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall, NMeta::TCIndices<tfp_Indices...>);
		};
		
		namespace NPrivate
		{
			template <typename t_CReturnType>
			struct TCCallSyncState
			{
				NThread::CEvent m_WaitEvent;
				TCAsyncResult<t_CReturnType> m_Result;
			};
		}

		template <typename t_CActor, typename t_CFunctor, typename t_CParams>
		struct TCActorCall
		{
			static_assert(!NTraits::TCIsReference<t_CActor>::mc_Value, "Incorrect type");
			static_assert(!NTraits::TCIsReference<t_CFunctor>::mc_Value, "Incorrect type");
			static_assert(!NTraits::TCIsReference<t_CParams>::mc_Value, "Incorrect type");
			
			using CReturnType = typename NPrivate::TCGetReturnType<typename NTraits::TCMemberFunctionPointerTraits<t_CFunctor>::CReturn>::CType;
			
			t_CActor mp_Actor;
			t_CFunctor mp_Functor;
			t_CParams mp_Params;
			TCActorCall(t_CActor const &_Actor, t_CFunctor const &_Functor, t_CParams const &_Params)
				: mp_Actor(_Actor)
				, mp_Functor(_Functor)
				, mp_Params(_Params)
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

			template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams>
			auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams> &&_OtherCall)
				-> TCActorCallPack<TCActorCall<t_CActor, t_CFunctor, t_CParams>, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams>> 
			{
				return TCActorCallPack<TCActorCall<t_CActor, t_CFunctor, t_CParams>, TCActorCall<tf_CActor, tf_CFunctor, tf_CParams>>
					(NContainer::fg_Tuple(fg_Move(*this), fg_Move(_OtherCall)))
				;
			}
			
			template <typename tf_CResultActor, typename tf_CResultFunctor>
			void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall)
			{
				mp_Actor->template f_Call<t_CFunctor>
					(
						NPrivate::fg_BindConstruct
						(
							fg_Move(mp_Functor)
							, fg_Move(mp_Params)

						)
						, fg_Move(_ResultCall.mp_Actor)
						, fg_Move(_ResultCall.mp_Functor)
					)
				;
#ifdef DMibContractConfigure_CheckEnabled
				mp_Actor.f_Clear();
#endif
			}

			template <typename tf_CFunctor, TCEnableIfType<!TCIsActorResultCall<tf_CFunctor>::mc_Value> * = nullptr>
			void operator > (tf_CFunctor &&_Functor)
			{
				auto pActor = mp_Actor->f_ConcurrencyManager().m_ThreadLocal->m_pCurrentActor;
				DMibFastCheck(pActor);
				*this > fg_ThisActor(pActor) / fg_Forward<tf_CFunctor>(_Functor);
			}

			CReturnType f_CallSync()
			{
				TCAsyncResult<CReturnType> Result;
				NThread::CEvent WaitEvent;
				*this > fg_ConcurrentActor() / [&](TCAsyncResult<CReturnType> &&_Result)
					{
						Result = fg_Move(_Result);
						WaitEvent.f_SetSignaled();
					}
				;

				WaitEvent.f_Wait();

				return Result.f_Move();
			}

			CReturnType f_CallSync(fp64 _Timeout)
			{
				NPtr::TCSharedPointer<NPrivate::TCCallSyncState<CReturnType>> pResult = fg_Construct();
				*this > fg_ConcurrentActor() / [pResult](TCAsyncResult<CReturnType> &&_Result)
					{
						pResult->m_Result = fg_Move(_Result);
						pResult->m_WaitEvent.f_SetSignaled();
					}
				;

				if (pResult->m_WaitEvent.f_WaitTimeout(_Timeout))
					DMibError("Timed out waiting for synchronous actor call to finish");

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
					NPtr::TCSharedPointer<TCCallMutipleActorStorage> pThis = fg_Explicit(this);
					m_Actor->f_QueueProcess
						(
							[pThis]()
							{
								auto &This = *pThis;
								auto &Internal = *This.m_Actor->fp_GetActor();
								CCurrentActorScope CurrentActor(Internal.f_ConcurrencyManager(), &Internal);
								This.m_Handler
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
			
			template <typename t_CActorCall>
			struct TCGetActorCallFunctionPointer;

			template 
			<	
				typename t_CActor
				, typename t_CFunctionPtr
				, typename t_CParams
			>
			struct TCGetActorCallFunctionPointer<TCActorCall<t_CActor, t_CFunctionPtr, t_CParams>>
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
			typedef NMeta::TCTypeList
				<
					typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType...
				> CTypeList
			;
			typedef NPrivate::TCCallMutipleActorStorage<tf_CResultFunctor, tf_CResultActor, CTypeList> CStorage;
			
			auto &ConcurrentActor = _ResultCall.mp_Actor->f_ConcurrencyManager().f_GetConcurrentActorForThisThread(_ResultCall.mp_Actor->f_GetPriority());
			
			NPtr::TCSharedPointer<CStorage> pStorage = fg_Construct(_ResultCall.mp_Actor, fg_Move(_ResultCall.mp_Functor));
			
			fg_Swallow
				(
					NContainer::fg_Get<tfp_Indices>(m_Calls).mp_Actor->template f_Call<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>
					(
						NPrivate::fg_BindConstruct
						(
							fg_Move(NContainer::fg_Get<tfp_Indices>(m_Calls).mp_Functor)
							, fg_Move(NContainer::fg_Get<tfp_Indices>(m_Calls).mp_Params)

						)
						, ConcurrentActor
						, [pStorage](TCAsyncResult<typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType> &&_Result)
						{
							auto &Internal = *pStorage;
							NContainer::fg_Get<tfp_Indices>(Internal.m_Results) = fg_Move(_Result);
							Internal.f_Finished();
						}
					)...
				)
			;
#ifdef DMibContractConfigure_CheckEnabled
			fg_Swallow([this]{NContainer::fg_Get<tfp_Indices>(m_Calls).mp_Actor.f_Clear(); return 0;}()...);
#endif
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
			mutable typename NPrivate::TCGetResultFunctorType<typename NTraits::TCRemoveReference<t_CResultFunctor>::CType, t_CRet>::CType m_ResultFunctor;
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
				auto &ConcurrencyManager = m_pActorInternal->f_ConcurrencyManager();
				auto &ThreadLocal = *ConcurrencyManager.m_ThreadLocal;
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
			void operator ()() const
			{
				if (m_Result.f_IsSet())
				{
					NPrivate::fg_CallResultFunctor(m_ResultFunctor, m_pResultActor->fp_GetActor(), fg_Move(m_Result));
				}
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
			inline_always bool f_ShouldDiscardResult() const
			{
				return NTraits::TCIsSame<t_CResultFunctor, NPrivate::CDiscardResultFunctor>::mc_Value;
			}
			~TCReportLocal()
			{
				if (f_ShouldDiscardResult())
					return;
				if (m_pActorInternal)
				{
					m_Result.f_SetException(DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted"));
					auto pActor = m_pResultActor;
					m_pActorInternal = nullptr;
					pActor->f_QueueProcess(fg_Move(*this));
				}
			}
		private:
			TCReportLocal(TCReportLocal const &_Other);
		};

	}
}
