// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
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
	struct TCFutureVector final
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

		struct CQueuedResult final
		{
			umint m_iResult;
			TCAsyncResult<t_CType> m_Result;
			CQueuedResult *m_pNext;
		};

		struct CInternal final
		{
			CInternal();
			~CInternal();
		public:
			friend struct TCFutureVector;

			TCFuture<NContainer::TCVector<TCAsyncResult<t_CType>>> f_GetResults();
			void fp_TransferResults();
			void fp_DestroyResults();

			NContainer::TCVector<TCAsyncResult<t_CType>> mp_Results;
			TCPromise<NContainer::TCVector<TCAsyncResult<t_CType>>> mp_GetResultsPromise;
			NAtomic::TCAtomic<bool> mp_bLazyResultsGotten = false;
			bool mp_bDefinedSize = false;
			align_cacheline umint mp_nAdded = 0;
			align_cacheline NStorage::CIntrusiveRefCount m_RefCount;
			NAtomic::TCAtomic<CQueuedResult *> mp_pFirstResult;
		};

		class CResultReceived final
		{
			umint mp_iResult;
			NStorage::TCSharedPointer<CInternal> mp_pInternal;
		public:
			CResultReceived(umint _iResult, NStorage::TCSharedPointer<CInternal> const &_pInternal);
			void operator ()(TCAsyncResult<t_CType> &&_Result) const;
		};

	public:
		TCFutureVector(umint _DefinedSize);
		TCFutureVector();
		~TCFutureVector();
		void f_SetLen(umint _DefinedSize);
		bool f_IsEmpty() const;

	private:
		CResultReceived fp_AddResult();

		auto fp_GetResults();
		TCFuture<NContainer::TCVector<t_CType>> fp_GetUnwrappedResults()
			requires(!NTraits::cIsVoid<t_CType>)
		;
		TCFuture<void> fp_GetUnwrappedResults()
			requires(NTraits::cIsVoid<t_CType>)
		;

		NStorage::TCSharedPointer<CInternal> mp_pInternal;
	};
}

#include "Malterlib_Concurrency_FutureVector.hpp"
