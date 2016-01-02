// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		
		template <typename t_CType>
		TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>> TCActorResultVector<t_CType>::CInternalActor::f_GetResults()
		{
			if (mp_bResultsGotten)
				DMibError("You have already gotten the results from this result vector once");

			mp_bResultsGotten = true;
			if (mp_nFinished == mp_nAdded.f_Load())
				mp_GetResultsContinuation.f_SetResult(fg_Move(mp_Results));
			return mp_GetResultsContinuation;
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::CResultReceived::CResultReceived(mint _iResult, TCActor<CInternalActor> const &_pActor)
			: m_iResult(_iResult)
			, mp_Actor(_pActor)
		{
		}

		template <typename t_CType>
		void TCActorResultVector<t_CType>::CResultReceived::operator ()(TCAsyncResult<t_CType> &&_Result) const
		{
			auto &Internal = mp_Actor->f_AccessInternal();
			auto iResult = m_iResult;
			mint CurLen = Internal.mp_Results.f_GetLen();
			while (CurLen <= iResult)
			{
				Internal.mp_Results.f_Insert();
				++CurLen;
			}
			Internal.mp_Results[iResult] = fg_Move(_Result);
			mint nFinished = ++Internal.mp_nFinished;
			if (Internal.mp_bResultsGotten && nFinished == Internal.mp_nAdded.f_Load())
				Internal.mp_GetResultsContinuation.f_SetResult(fg_Move(Internal.mp_Results));
		}

		template <typename t_CType>
		TCActorResultVector<t_CType>::TCActorResultVector()
			: mp_Actor(fg_ConstructActor<CInternalActor>())
		{
			
		}

		template <typename t_CType>
		TCActorResultCall<TCActor<typename TCActorResultVector<t_CType>::CInternalActor>, typename TCActorResultVector<t_CType>::CResultReceived> TCActorResultVector<t_CType>::f_AddResult()
		{
			auto &Internal = mp_Actor->f_AccessInternal();
			mint iResult = Internal.mp_nAdded.f_FetchAdd(1);
			
			return mp_Actor / CResultReceived(iResult, mp_Actor);
		}
		template <typename t_CType>
		auto TCActorResultVector<t_CType>::f_GetResults() -> decltype(fs_ActorType()(&CInternalActor::f_GetResults))
		{
			return mp_Actor(&CInternalActor::f_GetResults);
		}

		
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

	}
}
