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
	TCAsyncGenerator<t_CReturnType>::CIterator::CIterator(TCAsyncGenerator &&_Generator)
		: mp_pData(fg_Move(_Generator.mp_pData))
	{
	}

	template <typename t_CReturnType>
	auto TCAsyncGenerator<t_CReturnType>::f_GetIterator() && -> TCFuture<CIterator>
	{
		if (!mp_pData)
			co_return CIterator(fg_Move(*this));

		co_await ECoroutineFlag_AllowReferences;
		CIterator Iterator(fg_Move(*this));

		if (!Iterator.mp_pData->m_fGetNext)
			co_return DMibErrorInstance("Invalid async generator missing get next functor");

		Iterator.mp_pData->m_LastValue = co_await Iterator.mp_pData->m_fGetNext();
		co_return fg_Move(Iterator);
	}

	template <typename t_CReturnType>
	TCAsyncGenerator<t_CReturnType>::TCAsyncGenerator(TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> &&_fFunctor)
		: mp_pData(fg_Construct(fg_Move(_fFunctor)))
	{
	}

	template <typename t_CReturnType>
	TCFuture<void> TCAsyncGenerator<t_CReturnType>::f_Destroy() &&
	{
		if (!mp_pData)
			co_return {};

		co_await fg_Move(mp_pData->m_fGetNext).f_Destroy();

		co_return {};
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnType>
	bool TCAsyncGenerator<t_CReturnType>::f_HasData(void const *_pData) const
	{
		return true;
	}
#endif
}

#include "Malterlib_Concurrency_AsyncGeneratorPrivate.hpp"

