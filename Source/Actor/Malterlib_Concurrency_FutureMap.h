// Copyright © 2024 Unborken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CKey, typename t_CValue>
	struct TCFutureMap;

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &_Futures);

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &&_Futures);

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &_Futures);

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &&_Futures);

	template <typename tf_CKey>
	TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &_Futures);

	template <typename tf_CKey>
	TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &&_Futures);

	template <typename t_CKey, typename t_CValue, typename t_CForwardKey>
	struct TCFutureMapBoundKey
	{
		TCFutureMap<t_CKey, t_CValue> &m_This;
		t_CForwardKey m_Key;
	};

	template <typename t_CKey, typename t_CValue>
	struct TCFutureMap
	{
	private:
		template <typename tf_CKey, typename tf_CValue>
		friend TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &_Futures);
		template <typename tf_CKey, typename tf_CValue>
		friend TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(TCFutureMap<tf_CKey, tf_CValue> &&_Futures);

		template <typename tf_CKey, typename tf_CValue>
		friend TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &_Futures);
		template <typename tf_CKey, typename tf_CValue>
		friend TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(TCFutureMap<tf_CKey, tf_CValue> &&_Futures);

		template <typename tf_CKey>
		friend TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &_Futures);

		template <typename tf_CKey>
		friend TCFuture<NContainer::TCSet<tf_CKey>> fg_AllDone(TCFutureMap<tf_CKey, void> &&_Futures);

		template <typename t_CReturnValue2>
		friend struct TCFuture;

		template <typename t_CReturn2, typename t_CActor2, typename t_FFunctionPointer2, CBindActorOptions t_BindOptions2, bool t_bByValue2, typename ...tp_CParams2>
		friend struct TCBoundActorCall;

		struct CInternal
		{
			CInternal();
			~CInternal();
		public:
			NStorage::CIntrusiveRefCount m_RefCount;
			friend struct TCFutureMap;
			align_cacheline NAtomic::TCAtomic<mint> mp_nAdded;
			align_cacheline NAtomic::TCAtomic<mint> mp_nFinished;
			NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>> mp_Results;
			TCPromise<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> mp_GetResultsPromise{CPromiseConstructNoConsume()};

			TCFuture<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> f_GetResults();
			void fp_TransferResults();
		};

		class CResultReceived
		{
			TCAsyncResult<t_CValue> &mp_Result;
			NStorage::TCSharedPointer<CInternal> mp_pInternal;
		public:
			CResultReceived(TCAsyncResult<t_CValue> &_Result, NStorage::TCSharedPointer<CInternal> const &_pInternal);
			void operator ()(TCAsyncResult<t_CValue> &&_Result) const;
		};

	public:
		TCFutureMap();
		~TCFutureMap();
		bool f_IsEmpty() const;

		template <typename tf_CKey>
		auto operator [](tf_CKey &&_Key) -> TCFutureMapBoundKey<t_CKey, t_CValue, tf_CKey &&>;

	private:
		template <typename tf_CKey>
		CResultReceived fp_AddResult(tf_CKey &&_Key);

		auto fp_GetResults();

		TCFuture<NContainer::TCMap<t_CKey, t_CValue>> fp_GetUnwrappedResults()
			requires (!NTraits::TCIsVoid<t_CValue>::mc_Value)
		;

		TCFuture<NContainer::TCSet<t_CKey>> fp_GetUnwrappedResults()
			requires (NTraits::TCIsVoid<t_CValue>::mc_Value)
		;

		NStorage::TCSharedPointer<CInternal> mp_pInternal;
	};
}

#include "Malterlib_Concurrency_FutureMap.hpp"
