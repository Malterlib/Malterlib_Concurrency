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

			template <typename t_CType>
			struct TCIsContinuationWithError
			{
				enum
				{
					mc_Value = false
				};
			};

			template <typename t_CReturnValue, typename t_CError>
			struct TCIsContinuationWithError<TCContinuationWithError<t_CReturnValue, t_CError>>
			{
				enum
				{
					mc_Value = true
				};
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
				auto pResultActor = fg_GetResultActor(_Local.m_pResultActor);
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
				auto pResultActor = fg_GetResultActor(_Local.m_pResultActor);
				_Local.m_pActorInternal = nullptr;
				if (_Local.fs_ShouldDiscardResult())
					return;
				pResultActor->f_QueueProcess(fg_Move(fg_RemoveQualifiers(_Local)));
			}

			// Continuation result
			
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
				
				Continuation.f_OnResultSet
					(
						[Local = fg_Move(fg_RemoveQualifiers(_Local))](auto &&_Result) mutable
						{
							if (_Result.f_IsSet())
							{
		#if DMibConcurrencyDebugActorCallstacks
								auto Callstacks = fg_Move(Local.m_Result.m_Callstacks);
		#endif
								Local.m_Result = fg_Move(_Result);
		#if DMibConcurrencyDebugActorCallstacks
								Local.m_Result.m_Callstacks = fg_Move(Callstacks);
		#endif
							}
							else
								Local.m_Result.f_SetException(DMibImpExceptionInstance(NMib::NException::CException, "Result was not set"));

							auto pActor = fg_GetResultActor(Local.m_pResultActor);
							Local.m_pActorInternal = nullptr;
							pActor->f_QueueProcess(fg_Move(Local));
						}
					)
				;
				 
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
		}
	}
}
