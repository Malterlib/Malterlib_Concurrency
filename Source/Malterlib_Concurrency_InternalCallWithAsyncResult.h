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
			fg_CallWithAsyncResult(tf_CLocal &_Local, bool _bConcurrent)
			{
				//try
				{
#if DMibConcurrencyDebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					auto pActor = _Local.m_pThis->fp_GetActor();
					CCurrentActorScope CurrentActor(pActor);
					_Local.m_Result.f_SetResult(_Local.m_ToCall(*(pActor)));
				}
				//catch (...)
				//{
				//	_Local.m_Result.f_SetCurrentException();
				//}
				auto pActor = _Local.m_pResultActor;
				_Local.m_pThis = nullptr;
				if (_bConcurrent)
					pActor->f_QueueProcessConcurrent(fg_Move(fg_RemoveQualifiers(_Local)));
				else
					pActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));

			}
			
			template <typename t_CData, typename t_CLocal>
			struct TCCallWithAsyncResultHelper
			{					
				mutable t_CLocal m_Local;
				mutable t_CData m_pData;
				bool m_bConcurrent;
				TCCallWithAsyncResultHelper(t_CData _pData, t_CLocal &&_Local, bool _bConcurrent)
					: m_pData(_pData)
					, m_Local(fg_Move(_Local))
					, m_bConcurrent(_bConcurrent)
				{
				}
				TCCallWithAsyncResultHelper(TCCallWithAsyncResultHelper const &_Other)
					: m_Local(fg_Move(NMib::fg_RemoveQualifiers(_Other.m_Local)))
					, m_pData(fg_Move(NMib::fg_RemoveQualifiers(_Other.m_pData)))
					, m_bConcurrent(_Other.m_bConcurrent) 
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

					auto pActor = m_Local.m_pResultActor;
					m_Local.m_pThis = nullptr;
					if (m_bConcurrent)
						pActor->f_QueueProcessConcurrent(fg_Move(m_Local));
					else
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
			fg_CallWithAsyncResult(tf_CLocal &_Local, bool _bConcurrent)
			{
				//try
				{
#if DMibConcurrencyDebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					
					auto pActor = _Local.m_pThis->fp_GetActor();
					CCurrentActorScope CurrentActor(pActor);
					auto Continuation = _Local.m_ToCall(*pActor);
					auto pData = Continuation.m_pData.f_Get();
					pData->m_OnResult = TCCallWithAsyncResultHelper
						<
							decltype(pData)
							, typename NTraits::TCRemoveQualifiers<tf_CLocal>::CType
						>
						(pData, fg_Move(fg_RemoveQualifiers(_Local)), _bConcurrent)
					;
					if (pData->m_OnResultSet.f_FetchOr(2) & 1)
						pData->m_OnResult();

					return;
				}
				//catch (...)
				//{
				//	_Local.m_Result.f_SetCurrentException();
				//}
				auto pActor = _Local.m_pResultActor;
				_Local.m_pThis = nullptr;
				if (_bConcurrent)
					pActor->f_QueueProcessConcurrent(fg_Move(fg_RemoveQualifiers(_Local)));
				else
					pActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));
			}

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
			fg_CallWithAsyncResult(tf_CLocal &_Local, bool _bConcurrent)
			{
				//try
				{
#if DMibConcurrencyDebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					auto pActor = _Local.m_pThis->fp_GetActor();
					CCurrentActorScope CurrentActor(pActor);
					_Local.m_ToCall(*pActor);
					_Local.m_Result.f_SetResult();
				}
				//catch (...)
				//{
				//	_Local.m_Result.f_SetCurrentException();
				//}
				auto pActor = _Local.m_pResultActor;
				_Local.m_pThis = nullptr;
				if (_bConcurrent)
					pActor->f_QueueProcessConcurrent(fg_Move(fg_RemoveQualifiers(_Local)));
				else
					pActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));
			}

			template <typename tf_CResultFunctor, typename tf_CResultActor, typename tf_CResult>
			void fg_CallResultFunctor(tf_CResultFunctor &_ResultFunctor, tf_CResultActor _pResultActor, tf_CResult &&_Result)
			{
#if DMibConcurrencyDebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
				CCurrentActorScope CurrentActor(_pResultActor);
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
#if DMibConcurrencyDebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
				CCurrentActorScope CurrentActor(_pResultActor);
				_ResultFunctor(_pResultActor, fg_Forward<tf_CResult>(_Result));
			}
		}
	}
}
