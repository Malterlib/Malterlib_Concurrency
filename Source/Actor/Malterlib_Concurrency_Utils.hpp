// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		
		template <typename t_CType>
		TCActorResultVector<t_CType>::CInternal::CInternal()
		{
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::CInternal::~CInternal()
		{
		}
		
		namespace NPrivate
		{
			constexpr static mint const gc_ActorResultFinishedMask = DMibBitRangeTyped(0, sizeof(mint)*8-2, mint);
			constexpr static mint const gc_ActorResultResultsGottenMask = DMibBitRangeTyped(sizeof(mint)*8-1, sizeof(mint)*8-1, mint);
		}

		template <typename t_CType>
		TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>> TCActorResultVector<t_CType>::CInternal::f_GetResults()
		{
			mint nAdded = mp_nAdded.f_Load();

			if (!mp_bDefinedSize)
				mp_Results.f_SetLen(nAdded);
			
			mint nFinished = mp_nFinished.f_FetchOr(NPrivate::gc_ActorResultResultsGottenMask);
			
			if (nFinished & NPrivate::gc_ActorResultResultsGottenMask)
			{
				DMibFastCheck(false);
				DMibError("You have already gotten the results from this result vector once");
			}

			NAtomic::fg_CompilerFence();
			mp_bLazyResultsGotten = true;

			if ((nFinished & NPrivate::gc_ActorResultFinishedMask) == nAdded)
			{
				fp_TransferResults();
				mp_GetResultsContinuation.f_SetResult(fg_Move(mp_Results));
			}
			return mp_GetResultsContinuation;
		}
		
		template <typename t_CType>
		void TCActorResultVector<t_CType>::CInternal::fp_TransferResults()
		{
			auto *pResult = mp_pFirstResult.f_Exchange(nullptr);
			
			auto pResultsArray = mp_Results.f_GetArray();
			while (pResult)
			{
				pResultsArray[pResult->m_iResult] = fg_Move(pResult->m_Result);
				NPtr::TCUniquePointer<CQueuedResult> pResultDelete = fg_Explicit(pResult);
				pResult = pResult->m_pNext;
			}
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::CResultReceived::CResultReceived(mint _iResult, NPtr::TCSharedPointer<CInternal> const &_pInternal)
			: mp_iResult(_iResult)
			, mp_pInternal(_pInternal)
		{
		}

		template <typename t_CType>
		void TCActorResultVector<t_CType>::CResultReceived::operator ()(TCAsyncResult<t_CType> &&_Result) const
		{
			auto &Internal = *mp_pInternal;
			if (Internal.mp_bDefinedSize || Internal.mp_bLazyResultsGotten)
				Internal.mp_Results.f_GetArray()[mp_iResult] = fg_Move(_Result);
			else
			{
				NPtr::TCUniquePointer<CQueuedResult> pQueuedResult = fg_Construct();
				
				pQueuedResult->m_Result = fg_Move(_Result);
				pQueuedResult->m_iResult = mp_iResult;
				
				while (true)
				{
					CQueuedResult *pFirstResult = Internal.mp_pFirstResult.f_Load(NAtomic::EMemoryOrder_Relaxed);
					pQueuedResult->m_pNext = pFirstResult;
					if (Internal.mp_pFirstResult.f_CompareExchangeStrong(pFirstResult, pQueuedResult.f_Get()))
						break;
				}
				pQueuedResult.f_Detach();
			}
			
			mint nFinished = ++Internal.mp_nFinished;
			if ((nFinished & NPrivate::gc_ActorResultResultsGottenMask) && (nFinished & NPrivate::gc_ActorResultFinishedMask) == Internal.mp_nAdded.f_Load(NAtomic::EMemoryOrder_Relaxed))
			{
				Internal.fp_TransferResults();
				Internal.mp_GetResultsContinuation.f_SetResult(fg_Move(Internal.mp_Results));
			}
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::TCActorResultVector()
			: mp_pInternal(fg_Construct())
		{
			
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::TCActorResultVector(mint _DefinedSize)
			: mp_pInternal(fg_Construct())
		{
			mp_pInternal->mp_bDefinedSize = true;
			mp_pInternal->mp_Results.f_SetLen(_DefinedSize);
			
		}
		
		template <typename t_CType>
		void TCActorResultVector<t_CType>::f_SetLen(mint _DefinedSize)
		{
			DMibRequire(!mp_pInternal->mp_bDefinedSize);
			DMibRequire(mp_pInternal->mp_nAdded.f_Load() == 0);
			DMibRequire(mp_pInternal->mp_nFinished.f_Load() == 0);
			
			mp_pInternal->mp_bDefinedSize = true;
			mp_pInternal->mp_Results.f_SetLen(_DefinedSize);
		}
		
		template <typename t_CType>
		TCActorResultVector<t_CType>::~TCActorResultVector()
		{
			if (mp_pInternal)
				mp_pInternal.f_Clear();
		}

		template <typename t_CType>
		TCActorResultCall<TCActor<NPrivate::CDirectResultActor>, typename TCActorResultVector<t_CType>::CResultReceived> TCActorResultVector<t_CType>::f_AddResult()
		{
			auto &Internal = *mp_pInternal;
			DMibRequire(!(Internal.mp_nFinished.f_Load() & NPrivate::gc_ActorResultResultsGottenMask));
			mint iResult = Internal.mp_nAdded.f_FetchAdd(1);
			DMibRequire(!Internal.mp_bDefinedSize || iResult < Internal.mp_Results.f_GetLen());
			return NPrivate::fg_DirectResultActor() / CResultReceived(iResult, mp_pInternal);
		}
		
		template <typename t_CType>
		bool TCActorResultVector<t_CType>::f_IsEmpty() const
		{
			auto &Internal = *mp_pInternal;
			return Internal.mp_nAdded.f_Load() == 0;
		}

		template <typename t_CType>
		auto TCActorResultVector<t_CType>::f_GetResults() 
		{
			return fg_AnyConcurrentActor().f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>>>
					, [pInternal = mp_pInternal]() mutable -> TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>> 
					{
						return pInternal->f_GetResults();
					}
				)
			;
		}
		
		//
		// Map
		//
		
		template <typename t_CKey, typename t_CValue>
		TCContinuation<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> TCActorResultMap<t_CKey, t_CValue>::CInternalActor::f_GetResults()
		{
			if (mp_bResultsGotten)
				DMibError("You have already gotten the results from this result vector once");

			mp_bResultsGotten = true;
			if (mp_nFinished.f_Get() == mp_nAdded.f_Load())
				mp_GetResultsContinuation.f_SetResult(fg_Move(mp_Results));
			return mp_GetResultsContinuation;
		}

		template <typename t_CKey, typename t_CValue>
		template <typename tf_CKey>
		TCActorResultMap<t_CKey, t_CValue>::CResultReceived::CResultReceived(tf_CKey &&_Key, TCActor<CInternalActor> const &_pActor)
			: m_Key(fg_Forward<tf_CKey>(_Key))
			, mp_Actor(_pActor)
		{
		}

		template <typename t_CKey, typename t_CValue>
		void TCActorResultMap<t_CKey, t_CValue>::CResultReceived::operator ()(TCAsyncResult<t_CValue> &&_Result) const
		{
			auto &Internal = mp_Actor->f_AccessInternal();
			Internal.mp_Results[m_Key] = fg_Move(_Result);
			mint nFinished = ++Internal.mp_nFinished;
			if (Internal.mp_bResultsGotten && nFinished == Internal.mp_nAdded.f_Load())
				Internal.mp_GetResultsContinuation.f_SetResult(fg_Move(Internal.mp_Results));

		}


		template <typename t_CKey, typename t_CValue>
		TCActorResultMap<t_CKey, t_CValue>::TCActorResultMap()
			: mp_Actor(fg_ConstructActor<CInternalActor>())
		{
		}

		template <typename t_CKey, typename t_CValue>
		template <typename tf_CKey>
		TCActorResultCall<TCActor<typename TCActorResultMap<t_CKey, t_CValue>::CInternalActor>, typename TCActorResultMap<t_CKey, t_CValue>::CResultReceived> TCActorResultMap<t_CKey, t_CValue>::f_AddResult(tf_CKey &&_Key)
		{
			auto &Internal = mp_Actor->f_AccessInternal();
			Internal.mp_nAdded.f_FetchAdd(1);
							
			return TCActorResultCall<TCActor<CInternalActor>, CResultReceived>
				(
					mp_Actor
					, CResultReceived(fg_Forward<tf_CKey>(_Key), mp_Actor)
				)
			;
		}

		template <typename t_CKey, typename t_CValue>
		auto TCActorResultMap<t_CKey, t_CValue>::f_GetResults() -> decltype(fs_ActorType()(&CInternalActor::f_GetResults))
		{
			return mp_Actor(&CInternalActor::f_GetResults);
		}

		template <typename tf_CType, typename tf_FOnResult>
		void fg_CombineResults
			(
				NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
				DMibError(Errors);
		}

		template <typename tf_FOnResult>
		void fg_CombineResults
			(
				NContainer::TCVector<TCAsyncResult<void>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult();
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
				DMibError(Errors);
		}

		template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(fg_Move(*Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CReturn, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult();
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(fg_Move(*Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult();
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		
		template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CReturn, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, NContainer::TCVector<TCAsyncResult<void>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult();
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, NContainer::TCVector<TCAsyncResult<void>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult();
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}
		
	
	
		template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
		void fg_CombineResults
			(
				NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
				DMibError(Errors);
		}

		template <typename tf_CKey, typename tf_FOnResult>
		void fg_CombineResults
			(
				NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			for (auto &Result : _Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result));
				else
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
			if (!Errors.f_IsEmpty())
				DMibError(Errors);
		}

		template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(_Results->fs_GetKey(Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template <typename tf_CKey, typename tf_FOnResult>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult
			)
		{
			NStr::CStr Errors;
			if (!_Results)
				fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
			else
			{
				for (auto &Result : *_Results)
				{
					if (Result)
						_fOnResult(_Results->fs_GetKey(Result));
					else
						fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
			if (!Errors.f_IsEmpty())
			{
				_Continuation.f_SetException(DMibErrorInstance(Errors));
				return false;
			}
			return true;
		}

		template
		<
			typename tf_FToDispatch
			, TCEnableIfType<NPrivate::TCIsContinuation<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
			= nullptr
		>
		auto fg_Dispatch(TCActor<> const &_Actor, tf_FToDispatch &&_fDispatch)
		{
			using CReturnType = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType;
			
			return _Actor.f_CallByValue
				(
					&CActor::f_DispatchWithReturn<CReturnType>
					, NFunction::TCFunctionMovable<CReturnType ()>(fg_Forward<tf_FToDispatch>(_fDispatch))
				)
			;
		}

		template
		<
			typename tf_FToDispatch
			, TCEnableIfType<!NPrivate::TCIsContinuation<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
			= nullptr
		>
		auto fg_Dispatch(TCActor<> const &_Actor, tf_FToDispatch &&_fDispatch)
		{
			using CReturnType = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType;
			
			return _Actor.f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<CReturnType>>
					, NFunction::TCFunctionMovable<TCContinuation<CReturnType> ()>
					(
						[fDispatch = fg_Forward<tf_FToDispatch>(_fDispatch)]() mutable
						{
							return TCContinuation<CReturnType>::fs_RunProtected() > fg_Forward<tf_FToDispatch>(fDispatch);
						}
					)
				)
			;
		}
		
		template <typename tf_FToDispatch>
		auto fg_Dispatch(tf_FToDispatch &&_fDispatch)
		{
			return fg_Dispatch(fg_CurrentActor(), fg_Forward<tf_FToDispatch>(_fDispatch));
		}
		
		template <typename tf_FToDispatch>
		auto fg_ConcurrentDispatch(tf_FToDispatch &&_fDispatch)
		{
			return fg_Dispatch(fg_ConcurrentActor(), fg_Forward<tf_FToDispatch>(_fDispatch));
		}
		
		template <typename t_CReturnValue>
		template <typename tf_FResultHandler>
		auto TCContinuation<t_CReturnValue>::operator > (tf_FResultHandler &&_fResultHandler) const
		{
			return fg_Dispatch
				(
					[Continuation = *this]() mutable
					{
						return Continuation;
					}
				)
				> fg_Forward<tf_FResultHandler>(_fResultHandler)
			;
		}
	}
}
