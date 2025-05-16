// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CCanDestroyTracker
	{
		NStorage::CIntrusiveRefCount m_RefCount;
		TCPromise<void> m_Promise;

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
#include "Malterlib_Concurrency_FutureVector.h"
#include "Malterlib_Concurrency_FutureQueue.h"
