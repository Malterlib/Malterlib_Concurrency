// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Optional>
#include <Mib/Concurrency/ActorFunctor>

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Coroutine.h"
#include "Malterlib_Concurrency_AsyncGeneratorPrivate.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCAsyncGenerator
	{
		struct CIterator
		{
			CIterator(CIterator &&) = default;
			CIterator(CIterator const &) = delete;

			CIterator &operator = (CIterator &&) = default;
			CIterator &operator = (CIterator const &) = delete;

			explicit operator bool () const;

			TCFuture<void> operator ++ () noexcept;
			t_CReturnType &&operator * () noexcept;

			TCFuture<void> f_Destroy() &&;

		private:
			friend struct TCAsyncGenerator;

			CIterator(TCAsyncGenerator &&_Generator);

			NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorData<t_CReturnType>> mp_pData;
		};

		TCFuture<CIterator> f_GetIterator() &&;
		TCAsyncGenerator(TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> &&_fFunctor);

		TCAsyncGenerator() = default;

		TCAsyncGenerator(TCAsyncGenerator &&) = default;
		TCAsyncGenerator(TCAsyncGenerator const &) = delete;

		TCAsyncGenerator &operator = (TCAsyncGenerator &&) = default;
		TCAsyncGenerator &operator = (TCAsyncGenerator const &) = delete;

		TCFuture<void> f_Destroy() &&;

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

#if DMibEnableSafeCheck > 0
		bool f_HasData(void const *_pData) const;
#endif

	private:
		NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorData<t_CReturnType>> mp_pData;
	};

	struct CIsGeneratorAborted;

	extern CIsGeneratorAborted g_bShouldAbort;
}

#include "Malterlib_Concurrency_AsyncGenerator.hpp"
