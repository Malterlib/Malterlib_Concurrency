// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			template <typename t_CReturn>
			struct TCGetReturnType
			{
				typedef t_CReturn CType;
			};

			template <typename t_CReturn>
			struct TCGetReturnType<TCContinuation<t_CReturn>>
			{
				typedef t_CReturn CType;
			};

			template <typename t_CType>
			struct TCIsContinuation
			{
				enum
				{
					mc_Value = false
				};
			};

			template <typename t_CType>
			struct TCIsContinuation<TCContinuation<t_CType>>
			{
				enum
				{
					mc_Value = true
				};
			};

			template <typename t_CFunctorType, typename t_CRet, bool t_bIsMemberFunctionPtr = NTraits::TCIsMemberFunctionPointer<t_CFunctorType>::mc_Value>
			struct TCGetResultFunctorType
			{
				typedef NFunction::TCFunction
					<
						void (NFunction::CThisTag &, NConcurrency::TCAsyncResult<typename TCGetReturnType<t_CRet>::CType> &&_Result)
						, NFunction::CFunctionNoCopyTag
					> CType
				;
			};

			template <typename t_CFunctorType, typename t_CRet>
			struct TCGetResultFunctorType<t_CFunctorType, t_CRet, true>
			{
				typedef decltype((NFunction::fg_MemberFunctionFunctor(fg_GetType<t_CFunctorType>()))) CType;
			};

			// Direct result
			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				typename TCEnableIf
				<
					!TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					&& !NTraits::TCIsSame<tf_CResult, TCAsyncResult<void>>::mc_Value
					, void
				>::CType 
			fg_CallWithAsyncResult(tf_CLocal &_Local)
			{
				auto &ConcurrencyManager = *_Local.m_pActorInternal->mp_pConcurrencyManager;
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(ConcurrencyManager, pActor);
				{
#if DMibConcurrencyDebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(ConcurrencyManager, Callstack);
#endif
					if (_Local.fs_ShouldDiscardResult())
						_Local.m_ToCall(*(pActor));
					else
						_Local.m_Result.f_SetResult(_Local.m_ToCall(*(pActor)));
				}
				auto pResultActor = _Local.f_GetResultActor();
				_Local.m_pActorInternal = nullptr;
				if (_Local.fs_ShouldDiscardResult())
					return;
				pResultActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));
			}
			
			// Direct void result
			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				typename TCEnableIf
				<
					!TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					&& NTraits::TCIsSame<tf_CResult, TCAsyncResult<void>>::mc_Value
					, void
				>::CType 
			fg_CallWithAsyncResult(tf_CLocal &_Local)
			{
				auto &ConcurrencyManager = *_Local.m_pActorInternal->mp_pConcurrencyManager;
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(ConcurrencyManager, pActor);
				{
#if DMibConcurrencyDebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(ConcurrencyManager, Callstack);
#endif
					_Local.m_ToCall(*pActor);
					if (!_Local.fs_ShouldDiscardResult())
						_Local.m_Result.f_SetResult();
				}
				auto pResultActor = _Local.f_GetResultActor();
				_Local.m_pActorInternal = nullptr;
				if (_Local.fs_ShouldDiscardResult())
					return;
				pResultActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));
			}

			// Continuation result
			
			template <typename t_CData, typename t_CLocal>
			struct TCCallWithAsyncResultHelper
			{					
				mutable t_CLocal m_Local;
				mutable t_CData m_pData;
				TCCallWithAsyncResultHelper(t_CData _pData, t_CLocal &&_Local)
					: m_pData(_pData)
					, m_Local(fg_Move(_Local))
				{
				}
				TCCallWithAsyncResultHelper(TCCallWithAsyncResultHelper const &_Other)
					: m_Local(fg_Move(NMib::fg_RemoveQualifiers(_Other.m_Local)))
					, m_pData(fg_Move(NMib::fg_RemoveQualifiers(_Other.m_pData)))
				{
				}
				void operator ()() const
				{
					m_pData->m_OnResultSet.f_FetchOr(4);
					if (m_pData->m_Result.f_IsSet())
					{
#if DMibConcurrencyDebugActorCallstacks
						auto Callstacks = fg_Move(m_Local.m_Result.m_Callstacks);
#endif
						m_Local.m_Result = fg_Move(m_pData->m_Result);
#if DMibConcurrencyDebugActorCallstacks
						m_Local.m_Result.m_Callstacks = fg_Move(Callstacks);
#endif
					}
					else
						m_Local.m_Result.f_SetException(DMibImpExceptionInstance(NMib::NException::CException, "Result was not set"));

					auto pActor = m_Local.f_GetResultActor();
					m_Local.m_pActorInternal = nullptr;
					pActor->f_QueueProcess(fg_Move(m_Local));
				}
			};

			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				typename TCEnableIf
				<
					TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					, void
				>::CType 
			fg_CallWithAsyncResult(tf_CLocal &_Local)
			{
				auto &ConcurrencyManager = *_Local.m_pActorInternal->mp_pConcurrencyManager;
#if DMibConcurrencyDebugActorCallstacks
				auto &Callstack = _Local.m_Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(ConcurrencyManager, Callstack);
#endif
				
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(ConcurrencyManager, pActor);
				if (_Local.fs_ShouldDiscardResult())
				{
					_Local.m_ToCall(*pActor);
					return;
				}
				auto Continuation = _Local.m_ToCall(*pActor);
				auto pData = Continuation.m_pData.f_Get();
				
				pData->m_OnResult = TCCallWithAsyncResultHelper
					<
						decltype(pData)
						, typename NTraits::TCRemoveQualifiers<tf_CLocal>::CType
					>
					(pData, fg_Move(fg_RemoveQualifiers(_Local)))
				;
				if (pData->m_OnResultSet.f_FetchOr(2) & 1)
					pData->m_OnResult();
				return;
			}

			
			template <typename tf_CResultFunctor, typename tf_CResultActor, typename tf_CResult>
			void fg_CallResultFunctor(tf_CResultFunctor &_ResultFunctor, tf_CResultActor _pResultActor, tf_CResult &&_Result)
			{
				auto &ConcurrencyManager =_pResultActor->f_ConcurrencyManager();
#if DMibConcurrencyDebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(ConcurrencyManager, Callstack);
#endif
				CCurrentActorScope CurrentActor(ConcurrencyManager, _pResultActor);
				_ResultFunctor(fg_Forward<tf_CResult>(_Result));
			}

			template 
				<				
					typename tf_CFunction2
					, typename tf_CResultActor
					, typename tf_CResult
				>
			void fg_CallResultFunctor
				(
					NMib::NFunction::TCMemberFunctionFunctor<tf_CFunction2> &_ResultFunctor
					, tf_CResultActor _pResultActor
					, tf_CResult &&_Result
				)
			{
				auto &ConcurrencyManager =_pResultActor->f_ConcurrencyManager();
#if DMibConcurrencyDebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(ConcurrencyManager, Callstack);
#endif
				CCurrentActorScope CurrentActor(ConcurrencyManager, _pResultActor);
				_ResultFunctor(_pResultActor, fg_Forward<tf_CResult>(_Result));
			}
		}
	}
}
