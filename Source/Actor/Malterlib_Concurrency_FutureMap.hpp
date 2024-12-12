// Copyright © 2024 Unbroken AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &_Futures)
	{
		return _Futures.fp_GetResults();
	}

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &&_Futures)
	{
		return _Futures.fp_GetResults();
	}

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &&_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename tf_CKey>
	TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename tf_CKey>
	TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &&_Futures)
	{
		return _Futures.fp_GetUnwrappedResults();
	}

	template <typename t_CKey, typename t_CValue>
	TCFutureMap<t_CKey, t_CValue>::CInternal::CInternal()
	{
	}

	template <typename t_CKey, typename t_CValue>
	TCFutureMap<t_CKey, t_CValue>::CInternal::~CInternal()
	{
		mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));
	}

	template <typename t_CKey, typename t_CValue>
	TCFuture<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> TCFutureMap<t_CKey, t_CValue>::CInternal::f_GetResults()
	{
		return mp_GetResultsPromise.f_Future();
	}

	template <typename t_CKey, typename t_CValue>
	TCFutureMap<t_CKey, t_CValue>::CResultReceived::CResultReceived(TCAsyncResult<t_CValue> &_Result, NStorage::TCSharedPointer<CInternal> const &_pInternal)
		: mp_Result(_Result)
		, mp_pInternal(_pInternal)
	{
	}

	template <typename t_CKey, typename t_CValue>
	void TCFutureMap<t_CKey, t_CValue>::CResultReceived::operator ()(TCAsyncResult<t_CValue> &&_Result) const
	{
		mp_Result = fg_Move(_Result);
	}

	template <typename t_CKey, typename t_CValue>
	TCFutureMap<t_CKey, t_CValue>::TCFutureMap()
		: mp_pInternal(fg_Construct())
	{

	}

	template <typename t_CKey, typename t_CValue>
	TCFutureMap<t_CKey, t_CValue>::~TCFutureMap()
	{
		if (mp_pInternal)
			mp_pInternal.f_Clear();
	}

	template <typename t_CKey, typename t_CValue>
	template <typename tf_CKey>
	auto TCFutureMap<t_CKey, t_CValue>::operator [](tf_CKey &&_Key) -> TCFutureMapBoundKey<t_CKey, t_CValue, tf_CKey &&>
	{
		return {.m_This = *this, .m_Key = fg_Forward<tf_CKey>(_Key)};
	}

	template <typename t_CKey, typename t_CValue>
	template <typename tf_CKey>
	auto TCFutureMap<t_CKey, t_CValue>::fp_AddResult(tf_CKey &&_Key) -> CResultReceived
	{
		auto &Internal = *mp_pInternal;
		auto MapResult = Internal.mp_Results(fg_Forward<tf_CKey>(_Key));
		DMibFastCheck(MapResult.f_WasCreated());
		return CResultReceived(MapResult.f_GetResult(), mp_pInternal);
	}

	template <typename t_CKey, typename t_CValue>
	bool TCFutureMap<t_CKey, t_CValue>::f_IsEmpty() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.Internal.mp_Results.f_IsEmpty();
	}

	template <typename t_CKey, typename t_CValue>
	inline_always auto TCFutureMap<t_CKey, t_CValue>::fp_GetResults()
	{
		return fg_Exchange(mp_pInternal, nullptr)->f_GetResults();
	}

	template <typename t_CKey, typename t_CValue>
	TCFuture<NContainer::TCMap<t_CKey, t_CValue>> TCFutureMap<t_CKey, t_CValue>::fp_GetUnwrappedResults()
		requires (!NTraits::TCIsVoid<t_CValue>::mc_Value)
	{
		TCPromiseFuturePair<NContainer::TCMap<t_CKey, t_CValue>> Promise;

		fp_GetResults().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CKey, typename t_CValue>
	TCFuture<NContainer::TCSet<t_CKey>> TCFutureMap<t_CKey, t_CValue>::fp_GetUnwrappedResults()
		requires (NTraits::TCIsVoid<t_CValue>::mc_Value)
 	{
		TCPromiseFuturePair<NContainer::TCSet<t_CKey>> Promise;

		fp_GetResults().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> &&_Results)
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
