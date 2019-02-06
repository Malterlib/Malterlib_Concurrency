// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include "Malterlib_Concurrency_WeakActor.h"

namespace NMib::NConcurrency
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
	TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>> TCActorResultVector<t_CType>::CInternal::f_GetResults()
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
			mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));
		}
		return mp_GetResultsPromise;
	}

	template <typename t_CType>
	void TCActorResultVector<t_CType>::CInternal::fp_TransferResults()
	{
		auto *pResult = mp_pFirstResult.f_Exchange(nullptr);

		auto pResultsArray = mp_Results.f_GetArray();
		while (pResult)
		{
			pResultsArray[pResult->m_iResult] = fg_Move(pResult->m_Result);
			NStorage::TCUniquePointer<CQueuedResult> pResultDelete = fg_Explicit(pResult);
			pResult = pResult->m_pNext;
		}
	}

	template <typename t_CType>
	TCActorResultVector<t_CType>::CResultReceived::CResultReceived(mint _iResult, NStorage::TCSharedPointer<CInternal> const &_pInternal)
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
			NStorage::TCUniquePointer<CQueuedResult> pQueuedResult = fg_Construct();

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
			Internal.mp_GetResultsPromise.f_SetResult(fg_Move(Internal.mp_Results));
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
				&CActor::f_DispatchWithReturn<TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>>>
				, [pInternal = mp_pInternal]() mutable -> TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>>
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
	TCFuture<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> TCActorResultMap<t_CKey, t_CValue>::CInternalActor::f_GetResults()
	{
		if (mp_bResultsGotten)
			DMibError("You have already gotten the results from this result vector once");

		mp_bResultsGotten = true;
		if (mp_nFinished.f_Get() == mp_nAdded.f_Load())
			mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));
		return mp_GetResultsPromise;
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
			Internal.mp_GetResultsPromise.f_SetResult(fg_Move(Internal.mp_Results));

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

	template <typename t_CKey, typename t_CValue>
	bool TCActorResultMap<t_CKey, t_CValue>::f_IsEmpty() const
	{
		auto &Internal = mp_Actor->f_AccessInternal();
		return Internal.mp_nAdded.f_Load() == 0;
	}

	template <typename tf_CType, typename tf_FOnResult>
	void fg_CombineResults
		(
			NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
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
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
			DMibError(Errors);
	}

	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult();
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult();
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}


	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
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
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
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
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results->fs_GetKey(Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
			DMibError(Errors);
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		if (!_Results)
		{
			bIsError = true;
			fg_AddStrSep(Errors, _Results.f_GetExceptionStr(), DMibNewLine);
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result));
				else
				{
					bIsError = true;
					fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}


	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NStr::CStr Errors;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result));
			else
			{
				bIsError = true;
				fg_AddStrSep(Errors, Result.f_GetExceptionStr(), DMibNewLine);
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(DMibErrorInstance(Errors));
			return false;
		}
		return true;
	}
	template
	<
		typename tf_FToDispatch
		, TCEnableIfType<NPrivate::TCIsFuture<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
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
		, TCEnableIfType<!NPrivate::TCIsFuture<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
		= nullptr
	>
	auto fg_Dispatch(TCActor<> const &_Actor, tf_FToDispatch &&_fDispatch)
	{
		static_assert(!NPrivate::TCIsPromise<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value);
		using CReturnType = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType;

		return _Actor.f_CallByValue
			(
				&CActor::f_DispatchWithReturn<TCFuture<CReturnType>>
				, NFunction::TCFunctionMovable<TCFuture<CReturnType> ()>
				(
					[fDispatch = fg_Forward<tf_FToDispatch>(_fDispatch)]() mutable -> TCFuture<CReturnType>
					{
						return TCFuture<CReturnType>::fs_RunProtected() / fg_Forward<tf_FToDispatch>(fDispatch);
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

	template <typename tf_FToDispatch>
	auto fg_DirectDispatch(tf_FToDispatch &&_fDispatch)
	{
		return fg_Dispatch(fg_DirectCallActor(), fg_Forward<tf_FToDispatch>(_fDispatch));
	}

	template <typename t_CReturnValue>
	TCDispatchedActorCall<t_CReturnValue> TCFuture<t_CReturnValue>::f_Dispatch() const
	{
		return fg_DirectDispatch
			(
				[Future = *this]() mutable
				{
					return fg_Move(Future);
				}
			)
		;
	}

	template <typename t_CReturnValue>
	TCDispatchedActorCall<t_CReturnValue> TCFuture<t_CReturnValue>::f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit)
	{
		return f_Dispatch().f_Timeout(_Timeout, _TimeoutMessage, _bFireAtExit);
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_CallSync(fp64 _Timeout) const
	{
		return f_Dispatch().f_CallSync(_Timeout);
	}

	struct CDispatchHelperWithActor
	{
		CDispatchHelperWithActor(TCActor<> const &_Actor)
			: m_Actor(_Actor)
		{
		}

		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const
		{
			return fg_Dispatch(m_Actor, fg_Forward<tf_FFunction>(_fFunction));
		}

		TCActor<> m_Actor;
	};

	struct CDispatchHelperWithWeakActor
	{
		CDispatchHelperWithWeakActor(TCWeakActor<> const &_Actor)
			: m_Actor(_Actor)
		{
		}

		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const
		{
			return fg_Dispatch(m_Actor, fg_Forward<tf_FFunction>(_fFunction));
		}

		TCWeakActor<> m_Actor;
	};

	struct CDispatchHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const
		{
			return fg_Dispatch(fg_Forward<tf_FFunction>(_fFunction));
		}

		inline CDispatchHelperWithActor operator () (TCActor<> const &_Actor) const
		{
			return CDispatchHelperWithActor(_Actor);
		}

		inline CDispatchHelperWithWeakActor operator () (TCWeakActor<> const &_Actor) const
		{
			return CDispatchHelperWithWeakActor(_Actor);
		}
	};

	extern CDispatchHelper const &g_Dispatch;

	struct CConcurrentDispatchHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const
		{
			return fg_ConcurrentDispatch(fg_Forward<tf_FFunction>(_fFunction));
		}
	};

	extern CConcurrentDispatchHelper const &g_ConcurrentDispatch;

	struct CDirectDispatchHelper
	{
		template <typename tf_FFunction>
		inline auto operator / (tf_FFunction &&_fFunction) const
		{
			return fg_DirectDispatch(fg_Forward<tf_FFunction>(_fFunction));
		}
	};

	extern CDirectDispatchHelper const &g_DirectDispatch;

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler>
	void TCFuture<t_CReturnValue>::operator > (tf_FResultHandler &&_fResultHandler) const
	{
		fg_DirectDispatch
			(
				[Future = *this]() mutable
				{
					return fg_Move(Future);
				}
			)
			> fg_Forward<tf_FResultHandler>(_fResultHandler)
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_CReturnValue>
	void TCFuture<t_CReturnValue>::operator > (TCPromise<tf_CReturnValue> const &_Promise) const
	{
		fg_DirectDispatch
			(
				[Future = *this]() mutable
				{
					return fg_Move(Future);
				}
			)
			> _Promise
		;
	}

	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
	template <typename tf_CType>
	auto TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList>::operator + (TCFuture<tf_CType> const &_Future)
	{
		return *this + fg_DirectDispatch
			(
				[Future = _Future]() mutable
				{
					return fg_Move(Future);
				}
			)
		;
	}

	template <typename... tp_CCalls>
	template <typename tf_CType>
	auto TCActorCallPack<tp_CCalls...>::operator + (TCFuture<tf_CType> const &_Future)
	{
		return *this + fg_DirectDispatch
			(
				[Future = _Future]() mutable
				{
					return fg_Move(Future);
				}
			)
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_CType>
	auto TCFuture<t_CReturnValue>::operator + (TCFuture<tf_CType> const &_Other) const
	{
		return fg_DirectDispatch
			(
				[Future = *this]() mutable
				{
					return fg_Move(Future);
				}
			)
			+ _Other;
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
	auto TCFuture<t_CReturnValue>::operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall)
	{
		return fg_DirectDispatch
			(
				[Future = *this]() mutable
				{
					return fg_Move(Future);
				}
			)
			+ fg_Move(_ActorCall)
		;
	}
}
