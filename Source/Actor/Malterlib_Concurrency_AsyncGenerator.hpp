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

		TCPromise<void> Promise;
		mp_pData->m_fGetNext.f_CallDirect() > NPrivate::fg_DirectResultActor() / [pData = mp_pData, Promise](TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Result)
			{
				if (!_Result)
					return Promise.f_SetException(fg_Move(_Result));

				pData->m_LastValue = fg_Move(*_Result);
				Promise.f_SetResult();
			}
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnType>
	t_CReturnType &&TCAsyncGenerator<t_CReturnType>::CIterator::operator * () noexcept
	{
		DMibRequire(mp_pData && !!mp_pData->m_LastValue); // Already at end
		return fg_Move(*mp_pData->m_LastValue);
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::CIterator::f_Destroy() &&
	{
		TCPromise<void> Promise;

		if (!mp_pData)
			return Promise <<= g_Void;

		auto pData = mp_pData.f_Get();

		fg_Move(pData->m_fGetNext).f_Destroy() > [Promise, pData = fg_Move(mp_pData)](TCAsyncResult<void> &&_Result) mutable
			{
				pData.f_Clear();
				Promise.f_SetResult(fg_Move(_Result));
			}
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::CIterator::CIterator(TCAsyncGenerator &&_Generator)
		: mp_pData(fg_Move(_Generator.mp_pData))
	{
	}

	template <typename t_CReturnType>
	auto TCAsyncGenerator<t_CReturnType>::f_GetIterator() && -> TCFuture<CIterator>
	{
		TCPromise<CIterator> Promise;

		if (!mp_pData)
			return Promise <<= CIterator(fg_Move(*this));

		CIterator Iterator(fg_Move(*this));

		auto *pData = Iterator.mp_pData.f_Get();

		if (!pData->m_fGetNext)
			return Promise <<= DMibErrorInstance("Invalid async generator missing get next functor");

		pData->m_fGetNext.f_CallDirect() > NPrivate::fg_DirectResultActor() / [Promise, Iterator = fg_Move(Iterator)](TCAsyncResult<NStorage::TCOptional<t_CReturnType>> &&_Value) mutable
			{
				if (!_Value)
					return Promise.f_SetException(fg_Move(_Value));

				Iterator.mp_pData->m_LastValue = *_Value;

				Promise.f_SetResult(fg_Move(Iterator));
			}
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::TCAsyncGenerator(TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> &&_fFunctor)
		: mp_pData(fg_Construct(fg_Move(_fFunctor)))
	{
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::f_Destroy() &&
	{
		TCPromise<void> Promise;

		if (!mp_pData)
			return Promise <<= g_Void;

		auto pData = mp_pData.f_Get();

		fg_Move(pData->m_fGetNext).f_Destroy() > [Promise, pData = fg_Move(mp_pData)](TCAsyncResult<void> &&_Result) mutable
			{
				pData.f_Clear();
				Promise.f_SetResult(fg_Move(_Result));
			}
		;

		return Promise.f_MoveFuture();
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnType>
	bool TCAsyncGenerator<t_CReturnType>::f_HasData(void const *_pData) const
	{
		return true;
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
			mp_bIsAborted = _Handle.promise().m_pAborted->f_Load();
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
}

#include "Malterlib_Concurrency_AsyncGeneratorPrivate.hpp"

