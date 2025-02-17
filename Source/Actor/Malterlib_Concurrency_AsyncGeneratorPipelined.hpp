// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::operator bool () const
	{
		return mp_pData && !!mp_pData->m_LastValue;
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::operator ++ () noexcept
	{
		DMibRequire(mp_pData && !!mp_pData->m_LastValue); // Already at end
		auto pData = mp_pData.f_Get();

		TCFuture<NStorage::TCOptional<t_CReturnType>> Future;
		{
			DMibLock(pData->m_Lock);
			pData->f_QueueNext();
			Future = pData->m_QueuedResults.f_PopFirst();
		}

		TCPromiseFuturePair<void> Promise;
		Future.f_OnResultSet
			(
				[pData = mp_pData, Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Result)
				{
					if (!_Result)
						return Promise.f_SetException(fg_Move(_Result));

					pData->m_LastValue = fg_Move(*_Result);
					Promise.f_SetResult();
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	t_CReturnType &&TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::operator * () noexcept
	{
		DMibRequire(mp_pData && !!mp_pData->m_LastValue); // Already at end
		return fg_Move(*mp_pData->m_LastValue);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::~CPipelinedIterator()
	{
		if (!mp_pData)
			return;

		fg_Move(*this).f_Destroy().f_DiscardResult();
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::f_Destroy() &&
	{
		if (!mp_pData)
			return g_Void;

		TCPromiseFuturePair<void> Promise;

		auto &Data = *mp_pData;

		TCFuture<void> SubscriptionDestroyFuture;
		{
			DMibLock(Data.m_Lock);
			auto &Subscription = Data.m_fGetNext.f_GetSubscription();
			if (Subscription)
				SubscriptionDestroyFuture = fg_Exchange(Subscription, nullptr)->f_Destroy();
			else
				SubscriptionDestroyFuture = g_Void;
		}

		SubscriptionDestroyFuture.f_OnResultSet
			(
				[pData = fg_Move(mp_pData), Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<void> &&_SubscriptionResult) mutable
				{
					auto &Data = *pData;
					Data.f_AsyncDestroy().f_OnResultSet
						(
							[pData = fg_Move(pData), Promise = fg_Move(Promise), SubscriptionResult = fg_Move(_SubscriptionResult)](TCAsyncResult<void> &&_Result) mutable
							{
								TFAsyncGeneratorGetNext<t_CReturnType> fGetNext;
								{
									DMibLock(pData->m_Lock);
									pData->f_Destroy();
									fGetNext = fg_Move(pData->m_fGetNext);
								}

								fg_Move(fGetNext).f_Destroy().f_OnResultSet
									(
										[Promise = fg_Move(Promise), pData = fg_Move(pData), AsyncDestroyResult = fg_Move(_Result), SubscriptionResult = fg_Move(SubscriptionResult)]
										(TCAsyncResult<void> &&_Result) mutable
										{
											pData.f_Clear();
											NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
											if (!AsyncDestroyResult)
												ErrorCollector.f_AddError(AsyncDestroyResult.f_GetException());

											if (!SubscriptionResult)
												ErrorCollector.f_AddError(SubscriptionResult.f_GetException());

											if (!_Result)
												ErrorCollector.f_AddError(_Result.f_GetException());

											if (ErrorCollector.f_HasError())
												Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
											else
												Promise.f_SetResult();
										}
									)
								;
							}
						)
					;
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CPipelinedIterator::CPipelinedIterator(TCAsyncGenerator &&_Generator)
		: mp_pData(fg_Construct(fg_Move(*_Generator.mp_pData)))
	{
		_Generator.mp_pData.f_Clear();
	}

	template <typename t_CReturnType>
	void NPrivate::TCAsyncGeneratorDataPipelined<t_CReturnType>::f_Destroy()
	{
		TCAsyncGeneratorDataShared<t_CReturnType>::f_Destroy(m_bIsCoroutine);
		m_QueuedResults.f_Clear();
	}

	template <typename t_CReturnType>
	void NPrivate::TCAsyncGeneratorDataPipelined<t_CReturnType>::f_QueueNext()
	{
		m_QueuedResults.f_Insert(this->m_fGetNext.f_CallDirect());
		DMibLog(DMibAsyncGeneratorDebugLevel, "f_QueueNext {} Queued: {}", fg_GetTypeName<t_CReturnType>(), m_QueuedResults.f_GetLen());
	}

	template <typename t_CReturnType>
	auto TCAsyncGenerator<t_CReturnType>::f_GetPipelinedIterator(uint32 _PipelineLength) && -> TCFuture<CPipelinedIterator>
	{
		DMibFastCheck(_PipelineLength >= 1);

		if (!mp_pData)
			return DMibErrorInstance("Invalid async generator");

		if (!mp_pData->m_bSupportsPipelines)
			_PipelineLength = 1;

		DMibLog(DMibAsyncGeneratorDebugLevel, "f_GetPipelinedIterator({}) {} {}", _PipelineLength, fg_GetTypeName<t_CReturnType>(), mp_pData->m_bSupportsPipelines);

		CPipelinedIterator Iterator(fg_Move(*this));

		auto *pData = Iterator.mp_pData.f_Get();

		if (!pData->m_fGetNext)
			return DMibErrorInstance("Invalid async generator missing get next functor");

		if (pData->m_bIsCoroutine)
		{
			auto pFunctor = static_cast<NPrivate::TCAsyncGeneratorCoroutineFunctor<t_CReturnType> *>(pData->m_fGetNext.f_GetFunctor().f_GetFunctor());
			pFunctor->m_pRunState->m_PipelineLength = _PipelineLength;
		}

		TCFuture<NStorage::TCOptional<t_CReturnType>> FirstResultFuture;
		{
			DMibLock(pData->m_Lock);

			for (uint32 i = 0; i < _PipelineLength; ++i)
				pData->f_QueueNext();

			FirstResultFuture = pData->m_QueuedResults.f_PopFirst();
		}

		TCPromiseFuturePair<CPipelinedIterator> Promise;
		FirstResultFuture.f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise), Iterator = fg_Move(Iterator)](TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Result) mutable
				{
					if (!_Result)
						return Promise.f_SetException(fg_Move(_Result));

					Iterator.mp_pData->m_LastValue = fg_Move(*_Result);
					Promise.f_SetResult(fg_Move(Iterator));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}
}
