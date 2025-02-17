// Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Optional>
#include <Mib/Concurrency/ActorFunctor>

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Coroutine.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	using TFAsyncGeneratorGetNext = TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()>;
}

#define DMibAsyncGeneratorDebugLevel DebugVerbose1

#include "Malterlib_Concurrency_AsyncGeneratorPrivate.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCAsyncGenerator
	{
		struct CIterator
		{
			~CIterator();

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

		struct CPipelinedIterator
		{
			~CPipelinedIterator();

			CPipelinedIterator(CPipelinedIterator &&) = default;
			CPipelinedIterator(CPipelinedIterator const &) = delete;

			CPipelinedIterator &operator = (CPipelinedIterator &&) = default;
			CPipelinedIterator &operator = (CPipelinedIterator const &) = delete;

			explicit operator bool () const;

			TCFuture<void> operator ++ () noexcept;
			t_CReturnType &&operator * () noexcept;

			TCFuture<void> f_Destroy() &&;

		private:
			friend struct TCAsyncGenerator;

			CPipelinedIterator(TCAsyncGenerator &&_Generator);

			NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorDataPipelined<t_CReturnType>> mp_pData;
		};

		TCAsyncGenerator(TFAsyncGeneratorGetNext<t_CReturnType> &&_fFunctor, bool _bSupportPipelines, bool _bIsCoroutine);

		TCAsyncGenerator() = default;

		TCAsyncGenerator(TCAsyncGenerator &&) = default;
		TCAsyncGenerator(TCAsyncGenerator const &) = delete;

		TCAsyncGenerator &operator = (TCAsyncGenerator &&) = default;
		TCAsyncGenerator &operator = (TCAsyncGenerator const &) = delete;

		TCFuture<void> f_Destroy() &&;

		TCFuture<CIterator> f_GetSimpleIterator() &&;
		TCFuture<CPipelinedIterator> f_GetPipelinedIterator(uint32 _PipelineLength = 5) &&;

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorData<t_CReturnType>> const &f_Unsafe_PromiseData() const;

		bool f_SupportsPipelines() const;
		bool f_IsValid() const;

#if DMibEnableSafeCheck > 0
		bool f_Debug_HasData(void const *_pData) const;
		bool f_Debug_HadCoroutine(void const *_pData) const;
#endif

	private:
		NStorage::TCSharedPointer<NPrivate::TCAsyncGeneratorData<t_CReturnType>> mp_pData;
	};

	struct CIsGeneratorAborted;

	extern CIsGeneratorAborted g_bShouldAbort;

	struct CGetGeneratorPipelineLength;

	extern CGetGeneratorPipelineLength g_GetPipelineLength;
}

#include "Malterlib_Concurrency_AsyncGenerator.hpp"
#include "Malterlib_Concurrency_AsyncGeneratorPipelined.hpp"
