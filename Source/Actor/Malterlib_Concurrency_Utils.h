// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CCanDestroyTracker
	{
		NStorage::CIntrusiveRefCount m_RefCount;
		TCPromise<void> m_Promise{CPromiseConstructNoConsume()};

		CCanDestroyTracker();
		~CCanDestroyTracker();

		struct CCanDestroyResultFunctor
		{
			CCanDestroyResultFunctor(CCanDestroyResultFunctor &&);
			CCanDestroyResultFunctor(CCanDestroyResultFunctor const &);
			CCanDestroyResultFunctor(NStorage::TCSharedPointer<CCanDestroyTracker> &&_pThis);

			void operator () (TCAsyncResult<void> &&);

			NStorage::TCSharedPointer<CCanDestroyTracker> m_pThis;
		};

		TCFuture<void> f_Future() const;
		CCanDestroyResultFunctor f_Track();
	};

	template <typename t_CType>
	struct TCFutureVector;

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &_Futures);

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &&_Futures);

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &_Futures);

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &&_Futures);

	TCFuture<void> fg_AllDone(TCFutureVector<void> &_Futures);
	TCFuture<void> fg_AllDone(TCFutureVector<void> &&_Futures);

	template <typename t_CType>
	struct TCFutureVector
	{
	private:
		template <typename tf_CType>
		friend TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &_Futures);
		template <typename tf_CType>
		friend TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(TCFutureVector<tf_CType> &&_Futures);

		template <typename tf_CType>
		friend TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &_Futures);
		template <typename tf_CType>
		friend TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(TCFutureVector<tf_CType> &&_Futures);

		friend TCFuture<void> fg_AllDone(TCFutureVector<void> &_Futures);
		friend TCFuture<void> fg_AllDone(TCFutureVector<void> &&_Futures);

		template <typename t_CReturnValue2>
		friend struct TCFuture;

		template <typename t_CReturn2, typename t_CActor2, typename t_FFunctionPointer2, CBindActorOptions t_BindOptions2, bool t_bByValue2, typename ...tp_CParams2>
		friend struct TCBoundActorCall;

		struct CQueuedResult
		{
			mint m_iResult;
			TCAsyncResult<t_CType> m_Result;
			CQueuedResult *m_pNext;
		};

		struct CInternal
		{
			CInternal();
			~CInternal();
		public:
			NStorage::CIntrusiveRefCount m_RefCount;
			friend struct TCFutureVector;
			align_cacheline NAtomic::TCAtomic<mint> mp_nAdded;
			align_cacheline NAtomic::TCAtomic<mint> mp_nFinished;
			align_cacheline NAtomic::TCAtomic<CQueuedResult *> mp_pFirstResult;
			NContainer::TCVector<TCAsyncResult<t_CType>> mp_Results;
			TCPromise<NContainer::TCVector<TCAsyncResult<t_CType>>> mp_GetResultsPromise{CPromiseConstructNoConsume()};
			bool mp_bDefinedSize = false;
			NAtomic::TCAtomic<bool> mp_bLazyResultsGotten = false;

			TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>> f_GetResults();
			void fp_TransferResults();
		};

		class CResultReceived
		{
			mint mp_iResult;
			NStorage::TCSharedPointer<CInternal> mp_pInternal;
		public:
			CResultReceived(mint _iResult, NStorage::TCSharedPointer<CInternal> const &_pInternal);
			void operator ()(TCAsyncResult<t_CType> &&_Result) const;
		};

	public:
		TCFutureVector(mint _DefinedSize);
		TCFutureVector();
		~TCFutureVector();
		void f_SetLen(mint _DefinedSize);
		bool f_IsEmpty() const;

	private:
		CResultReceived fp_AddResult();

		auto fp_GetResults();
		TCFuture<NContainer::TCVector<t_CType>> fp_GetUnwrappedResults()
			requires(!NTraits::TCIsVoid<t_CType>::mc_Value)
		;
		TCFuture<void> fp_GetUnwrappedResults()
			requires(NTraits::TCIsVoid<t_CType>::mc_Value)
		;

		NStorage::TCSharedPointer<CInternal> mp_pInternal;
	};

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(NContainer::TCVector<TCFuture<tf_CType>> &_Futures);

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(NContainer::TCVector<TCFuture<tf_CType>> &_Futures);

	template <typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
	void fg_CombineResults
		(
			NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
		)
	;

	template <typename tf_FOnResult = NFunction::TCFunction<void ()>>
	void fg_CombineResults
		(
			NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = [](){}
		)
	;

	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
		)
	;

	template <typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void ()>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult = []{}
		)
	;

	template <typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
		)
	;

	template <typename tf_FOnResult = NFunction::TCFunction<void ()>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult = []{}
		)
	;

	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
		)
	;

	template <typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void ()>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = []{}
		)
	;

	template <typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
		)
	;

	template <typename tf_FOnResult = NFunction::TCFunction<void ()>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = []{}
		)
	;

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &, tf_CType const &)>>
	void fg_CombineResults
		(
			NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
		)
	;

	template <typename tf_CKey, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &)>>
	void fg_CombineResults
		(
			NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
		)
	;

	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &, tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
		)
	;

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
		)
	;

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &, tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
		)
	;

	template <typename tf_CKey, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
		)
	;

	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &, tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
		)
	;

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &)>>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
		)
	;

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &, tf_CType const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
		)
	;

	template <typename tf_CKey, typename tf_FOnResult = NFunction::TCFunction<void (tf_CKey const &)>>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
		)
	;
}

#include "Malterlib_Concurrency_Unwrap.h"
#include "Malterlib_Concurrency_Utils.hpp"

#include "Malterlib_Concurrency_FutureMap.h"

