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
	}

	template <typename t_CKey, typename t_CValue>
	TCFuture<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> TCFutureMap<t_CKey, t_CValue>::CInternal::f_GetResults()
	{
		mint nAdded = mp_nAdded.f_Load();
		mint nFinished = mp_nFinished.f_FetchOr(NPrivate::gc_ActorResultResultsGottenMask);

		if (nFinished & NPrivate::gc_ActorResultResultsGottenMask)
		{
			// You have already gotten the results from this future map already
			DMibPDebugBreak;
		}

		if ((nFinished & NPrivate::gc_ActorResultFinishedMask) == nAdded)
			mp_GetResultsPromise.f_SetResult(fg_Move(mp_Results));

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

		auto &Internal = *mp_pInternal;
		mint nFinished = ++Internal.mp_nFinished;
		if ((nFinished & NPrivate::gc_ActorResultResultsGottenMask) && (nFinished & NPrivate::gc_ActorResultFinishedMask) == Internal.mp_nAdded.f_Load(NAtomic::EMemoryOrder_Relaxed))
			Internal.mp_GetResultsPromise.f_SetResult(fg_Move(Internal.mp_Results));
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
		DMibRequire(!(Internal.mp_nFinished.f_Load() & NPrivate::gc_ActorResultResultsGottenMask));
		Internal.mp_nAdded.f_FetchAdd(1);
		auto MapResult = Internal.mp_Results(fg_Forward<tf_CKey>(_Key));
		DMibFastCheck(MapResult.f_WasCreated());
		return CResultReceived(MapResult.f_GetResult(), mp_pInternal);
	}

	template <typename t_CKey, typename t_CValue>
	bool TCFutureMap<t_CKey, t_CValue>::f_IsEmpty() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.mp_nAdded.f_Load() == 0;
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
		TCPromise<NContainer::TCMap<t_CKey, t_CValue>> Promise{CPromiseConstructNoConsume()};

		fp_GetResults().f_OnResultSet
			(
				[Promise](TCAsyncResult<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CKey, typename t_CValue>
	TCFuture<NContainer::TCSet<t_CKey>> TCFutureMap<t_CKey, t_CValue>::fp_GetUnwrappedResults()
		requires (NTraits::TCIsVoid<t_CValue>::mc_Value)
 	{
		TCPromise<NContainer::TCSet<t_CKey>> Promise{CPromiseConstructNoConsume()};

		fp_GetResults().f_OnResultSet
			(
				[Promise](TCAsyncResult<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> &&_Results)
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
