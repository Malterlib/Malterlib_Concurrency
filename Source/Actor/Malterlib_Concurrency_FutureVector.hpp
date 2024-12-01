// Copyright © 2024 Unbroken AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::operator > (TCFutureVector<t_CReturnValue> &_FutureVector) &&
	{
		auto This = fg_Move(*this);
		This.f_OnResultSet(_FutureVector.fp_AddResult());
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &_Futures)
	{
		return _Futures.fp_GetResults();
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &&_Futures)
	{
		return _Futures.fp_GetResults();
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &&_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename t_CType>
	TCFutureVector<t_CType>::CInternal::CInternal()
	{
	}

	template <typename t_CType>
	TCFutureVector<t_CType>::CInternal::~CInternal()
	{
	}

	template <typename t_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>> TCFutureVector<t_CType>::CInternal::f_GetResults()
	{
		mint nAdded = mp_nAdded.f_Load();

		if (!mp_bDefinedSize)
			mp_Results.f_SetLen(nAdded);

		mint nFinished = mp_nFinished.f_FetchOr(NPrivate::gc_ActorResultResultsGottenMask);

		if (nFinished & NPrivate::gc_ActorResultResultsGottenMask)
		{
			// You have already gotten the results from this future vector already
			DMibPDebugBreak;
		}

		mp_bLazyResultsGotten.f_Store(true);

		if ((nFinished & NPrivate::gc_ActorResultFinishedMask) == nAdded)
		{
			fp_TransferResults();
			mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));
		}
		return mp_GetResultsPromise.f_Future();
	}

	template <typename t_CType>
	void TCFutureVector<t_CType>::CInternal::fp_TransferResults()
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
	TCFutureVector<t_CType>::CResultReceived::CResultReceived(mint _iResult, NStorage::TCSharedPointer<CInternal> const &_pInternal)
		: mp_iResult(_iResult)
		, mp_pInternal(_pInternal)
	{
	}

	template <typename t_CType>
	void TCFutureVector<t_CType>::CResultReceived::operator ()(TCAsyncResult<t_CType> &&_Result) const
	{
		auto &Internal = *mp_pInternal;
		if (Internal.mp_bDefinedSize || Internal.mp_bLazyResultsGotten.f_Load(NAtomic::EMemoryOrder_Acquire))
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
	TCFutureVector<t_CType>::TCFutureVector()
		: mp_pInternal(fg_Construct())
	{

	}

	template <typename t_CType>
	TCFutureVector<t_CType>::TCFutureVector(mint _DefinedSize)
		: mp_pInternal(fg_Construct())
	{
		mp_pInternal->mp_bDefinedSize = true;
		mp_pInternal->mp_Results.f_SetLen(_DefinedSize);

	}

	template <typename t_CType>
	void TCFutureVector<t_CType>::f_SetLen(mint _DefinedSize)
	{
		DMibRequire(!mp_pInternal->mp_bDefinedSize);
		DMibRequire(mp_pInternal->mp_nAdded.f_Load() == 0);
		DMibRequire(mp_pInternal->mp_nFinished.f_Load() == 0);

		mp_pInternal->mp_bDefinedSize = true;
		mp_pInternal->mp_Results.f_SetLen(_DefinedSize);
	}

	template <typename t_CType>
	TCFutureVector<t_CType>::~TCFutureVector()
	{
		if (mp_pInternal)
			mp_pInternal.f_Clear();
	}

	template <typename t_CType>
	auto TCFutureVector<t_CType>::fp_AddResult() -> CResultReceived
	{
		auto &Internal = *mp_pInternal;
		DMibRequire(!(Internal.mp_nFinished.f_Load() & NPrivate::gc_ActorResultResultsGottenMask));
		mint iResult = Internal.mp_nAdded.f_FetchAdd(1);
		DMibRequire(!Internal.mp_bDefinedSize || iResult < Internal.mp_Results.f_GetLen());
		return CResultReceived(iResult, mp_pInternal);
	}

	template <typename t_CType>
	bool TCFutureVector<t_CType>::f_IsEmpty() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.mp_nAdded.f_Load() == 0;
	}

	template <typename t_CType>
	inline_always auto TCFutureVector<t_CType>::fp_GetResults()
	{
		return fg_Exchange(mp_pInternal, nullptr)->f_GetResults();
	}

	template <typename t_CType>
	TCFuture<NContainer::TCVector<t_CType>> TCFutureVector<t_CType>::fp_GetUnwrappedResults()
		requires(!NTraits::TCIsVoid<t_CType>::mc_Value)
	{
		TCPromise<NContainer::TCVector<t_CType>> Promise{CPromiseConstructNoConsume()};

		fp_GetResults().f_OnResultSet
			(
				[Promise](TCAsyncResult<NContainer::TCVector<TCAsyncResult<t_CType>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CType>
	TCFuture<void> TCFutureVector<t_CType>::fp_GetUnwrappedResults()
		requires(NTraits::TCIsVoid<t_CType>::mc_Value)
	{
		TCPromise<void> Promise{CPromiseConstructNoConsume()};

		fp_GetResults().f_OnResultSet
			(
				[Promise](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return Promise.f_MoveFuture();
	}	
}
