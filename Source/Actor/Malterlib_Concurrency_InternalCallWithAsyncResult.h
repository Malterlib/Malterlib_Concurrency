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

			template <typename t_CReturnValue>
			struct TCIsContinuationWithError<TCContinuationWithError<t_CReturnValue>>
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
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(pActor);
				{
#if DMibConfig_Concurrency_DebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					if (_Local.fs_ShouldDiscardResult())
						_Local.m_ToCall(*(pActor));
					else
						_Local.m_Result.f_SetResult(_Local.m_ToCall(*(pActor)));
				}
				fg_RemoveQualifiers(_Local).template f_ResultAvailable<>();
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
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(pActor);
				{
#if DMibConfig_Concurrency_DebugActorCallstacks
					auto &Callstack = _Local.m_Result.m_Callstacks;
					CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
					_Local.m_ToCall(*pActor);
					if (!_Local.fs_ShouldDiscardResult())
						_Local.m_Result.f_SetResult();
				}
				fg_RemoveQualifiers(_Local).template f_ResultAvailable<>();
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
#if DMibConfig_Concurrency_DebugActorCallstacks
				auto &Callstack = _Local.m_Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
				
				auto pActor = _Local.m_pActorInternal->fp_GetActor();
				CCurrentActorScope CurrentActor(pActor);
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
		#if DMibConfig_Concurrency_DebugActorCallstacks
								auto Callstacks = fg_Move(Local.m_Result.m_Callstacks);
		#endif
								Local.m_Result = fg_Move(_Result);
		#if DMibConfig_Concurrency_DebugActorCallstacks
								Local.m_Result.m_Callstacks = fg_Move(Callstacks);
		#endif
							}
							else
								Local.m_Result.f_SetException(DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "Result was not set"));

							fg_RemoveQualifiers(Local).template f_ResultAvailable<>();
						}
					)
				;
				 
				return;
			}

			
			template <typename tf_CResultFunctor, typename tf_CResultActor, typename tf_CResult>
			void fg_CallResultFunctor(tf_CResultFunctor &_ResultFunctor, tf_CResultActor _pResultActor, tf_CResult &&_Result)
			{
#if DMibConfig_Concurrency_DebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
				CCurrentActorScope CurrentActor(_pResultActor);
				_ResultFunctor(fg_Forward<tf_CResult>(_Result));
			}

			template <typename tf_CResultFunctor, typename tf_CResult>
			void fg_CallResultFunctorDirect(tf_CResultFunctor &_ResultFunctor, tf_CResult &&_Result)
			{
#if DMibConfig_Concurrency_DebugActorCallstacks
				auto &Callstack = _Result.m_Callstacks;
				CAsyncCallstacksScope CallstacksScope(Callstack);
#endif
				CCurrentActorScope CurrentActor(nullptr);
				_ResultFunctor(fg_Forward<tf_CResult>(_Result));
			}
		}
	}
}
