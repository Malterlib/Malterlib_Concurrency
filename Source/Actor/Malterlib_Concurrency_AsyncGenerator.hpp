// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CIterator::operator bool () const
	{
		return mp_pData && !!mp_pData->m_LastValue;
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::CIterator::operator ++ () noexcept
	{
		DMibRequire(mp_pData && !!mp_pData->m_LastValue); // Already at end

		TCPromiseFuturePair<void> Promise;
		mp_pData->m_fGetNext.f_CallDirect().f_OnResultSet
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
	t_CReturnType &&TCAsyncGenerator<t_CReturnType>::CIterator::operator * () noexcept
	{
		DMibRequire(mp_pData && !!mp_pData->m_LastValue); // Already at end
		return fg_Move(*mp_pData->m_LastValue);
	}

	template <typename t_CReturnType>
	void NPrivate::TCAsyncGeneratorData<t_CReturnType>::f_Destroy()
	{
		TCAsyncGeneratorDataShared<t_CReturnType>::f_Destroy(m_bIsCoroutine);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CIterator::~CIterator()
	{
		if (!mp_pData)
			return;

		fg_Move(*this).f_Destroy().f_DiscardResult();
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::CIterator::f_Destroy() &&
	{
		if (!mp_pData)
			return g_Void;

		auto pData = mp_pData.f_Get();

		TCPromiseFuturePair<void> Promise;
		fg_Move(pData->m_fGetNext).f_Destroy().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise), pData = fg_Move(mp_pData)](TCAsyncResult<void> &&_Result) mutable
				{
					pData->f_Destroy();
					pData.f_Clear();
					Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CIterator::CIterator(TCAsyncGenerator &&_Generator)
		: mp_pData(fg_Move(_Generator.mp_pData))
	{
	}

	template <typename t_CReturnType>
	bool TCAsyncGenerator<t_CReturnType>::f_SupportsPipelines() const
	{
		return mp_pData->m_bSupportsPipelines;
	}

	template <typename t_CReturnType>
	auto TCAsyncGenerator<t_CReturnType>::f_GetSimpleIterator() && -> TCFuture<CIterator>
	{
		if (!mp_pData)
			return CIterator(fg_Move(*this));

		CIterator Iterator(fg_Move(*this));

		auto *pData = Iterator.mp_pData.f_Get();

		if (!pData->m_fGetNext)
			return DMibErrorInstance("Invalid async generator missing get next functor");

		DMibLog(DMibAsyncGeneratorDebugLevel, "f_GetSimpleIterator {}", fg_GetTypeName<t_CReturnType>());

		TCPromiseFuturePair<CIterator> Promise;
		pData->m_fGetNext.f_CallDirect().f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise), Iterator = fg_Move(Iterator)](TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Value) mutable
				{
					if (!_Value)
						return Promise.f_SetException(fg_Move(_Value));

					Iterator.mp_pData->m_LastValue = fg_Move(*_Value);

					Promise.f_SetResult(fg_Move(Iterator));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::TCAsyncGenerator(TFAsyncGeneratorGetNext<t_CReturnType> &&_fFunctor, bool _bSupportPipelines, bool _bIsCoroutine)
		: mp_pData(fg_Construct(fg_Move(_fFunctor), _bSupportPipelines, _bIsCoroutine))
	{
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::f_Destroy() &&
	{
		if (!mp_pData)
			return g_Void;

		auto pData = mp_pData.f_Get();

		TCPromiseFuturePair<void> Promise;
		fg_Move(pData->m_fGetNext).f_Destroy() > [Promise = fg_Move(Promise.m_Promise), pData = fg_Move(mp_pData)](TCAsyncResult<void> &&_Result) mutable
			{
				pData.f_Clear();
				Promise.f_SetResult(fg_Move(_Result));
			}
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorData<t_CReturnType>> const &TCAsyncGenerator<t_CReturnType>::f_Unsafe_PromiseData() const
	{
		return mp_pData;
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnType>
	bool TCAsyncGenerator<t_CReturnType>::f_Debug_HasData(void const *_pData) const
	{
		return true;
	}

	template <typename t_CReturnType>
	bool TCAsyncGenerator<t_CReturnType>::f_Debug_HadCoroutine(void const *_pData) const
	{
		return mp_pData->m_pHadCoroutine == _pData;
	}
#endif

	struct [[nodiscard("You need to co_await to get the result")]] CIsGeneratorAbortedAwaiter
	{
		bool await_ready() noexcept
		{
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			mp_bIsAborted = _Handle.promise().m_pRunState->m_bAborted.f_Load();
			return false;
		}

		bool await_resume()
		{
			return mp_bIsAborted;
		}

	private:
		bool mp_bIsAborted = false;
	};

	struct [[nodiscard("You need to co_await to get the result")]] CIsGeneratorAborted
	{
		auto operator co_await()
		{
			return CIsGeneratorAbortedAwaiter();
		}
	};

	struct [[nodiscard("You need to co_await to get the result")]] CGetGeneratorPipelineLengthAwaiter
	{
		bool await_ready() noexcept
		{
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			mp_PipelineLength = _Handle.promise().m_pRunState->m_PipelineLength;
			return false;
		}

		uint32 await_resume()
		{
			return mp_PipelineLength;
		}

	private:
		uint32 mp_PipelineLength = false;
	};

	struct [[nodiscard("You need to co_await to get the result")]] CGetGeneratorPipelineLength
	{
		auto operator co_await()
		{
			return CGetGeneratorPipelineLengthAwaiter();
		}
	};
}

#include "Malterlib_Concurrency_AsyncGeneratorPrivate.hpp"

