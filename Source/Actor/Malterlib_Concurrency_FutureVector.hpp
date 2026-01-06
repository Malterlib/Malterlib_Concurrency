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
		if (!mp_bLazyResultsGotten.f_Load(NAtomic::EMemoryOrder_Acquire)) [[unlikely]]
		{
			fp_DestroyResults();

			mp_GetResultsPromise.f_Abandon
				(
					[]() -> NException::CExceptionPointer
					{
						return CAsyncResult::fs_ResultWasNotSetException();
					}
				)
			;

			return;
		}

		fp_TransferResults();
		mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));
	}

	template <typename t_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>> TCFutureVector<t_CType>::CInternal::f_GetResults()
	{
		if (!mp_bDefinedSize)
			mp_Results.f_SetLen(mp_nAdded);

		mp_bLazyResultsGotten.f_Store(true, NAtomic::EMemoryOrder_Release);

		return mp_GetResultsPromise.f_Future();
	}

	template <typename t_CType>
	void TCFutureVector<t_CType>::CInternal::fp_DestroyResults()
	{
		auto *pResult = mp_pFirstResult.f_Exchange(nullptr, NAtomic::EMemoryOrder_Acquire);

		while (pResult)
		{
			auto pNextResult = pResult->m_pNext;
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pResult);
			pResult = pNextResult;
		}
	}

	template <typename t_CType>
	void TCFutureVector<t_CType>::CInternal::fp_TransferResults()
	{
		auto *pResult = mp_pFirstResult.f_Exchange(nullptr, NAtomic::EMemoryOrder_Acquire);

		auto pResultsArray = mp_Results.f_GetArray();
		while (pResult)
		{
			pResultsArray[pResult->m_iResult] = fg_Move(pResult->m_Result);
			auto pNextResult = pResult->m_pNext;
			fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), pResult);
			pResult = pNextResult;
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
				if (Internal.mp_pFirstResult.f_CompareExchangeStrong(pFirstResult, pQueuedResult.f_Get(), NAtomic::EMemoryOrder_Release, NAtomic::EMemoryOrder_Relaxed))
					break;
			}

			pQueuedResult.f_Detach();
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
		DMibRequire(mp_pInternal->mp_nAdded == 0);

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
		mint iResult = Internal.mp_nAdded++;
		DMibRequire(!Internal.mp_bDefinedSize || iResult < Internal.mp_Results.f_GetLen());
		return CResultReceived(iResult, mp_pInternal);
	}

	template <typename t_CType>
	bool TCFutureVector<t_CType>::f_IsEmpty() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.mp_nAdded == 0;
	}

	template <typename t_CType>
	inline_always auto TCFutureVector<t_CType>::fp_GetResults()
	{
		return fg_Exchange(mp_pInternal, nullptr)->f_GetResults();
	}

	template <typename t_CType>
	TCFuture<NContainer::TCVector<t_CType>> TCFutureVector<t_CType>::fp_GetUnwrappedResults()
		requires(!NTraits::cIsVoid<t_CType>)
	{
		TCPromiseFuturePair<NContainer::TCVector<t_CType>> Promise;

		fp_GetResults().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCVector<TCAsyncResult<t_CType>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CType>
	TCFuture<void> TCFutureVector<t_CType>::fp_GetUnwrappedResults()
		requires(NTraits::cIsVoid<t_CType>)
	{
		TCPromiseFuturePair<void> Promise;

		fp_GetResults().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}
}
