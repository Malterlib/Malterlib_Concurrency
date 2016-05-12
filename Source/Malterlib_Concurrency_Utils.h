// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		struct CCanDestroyTracker
		{
			TCContinuation<void> m_Continuation;
			~CCanDestroyTracker()
			{
				if (!m_Continuation.f_IsSet())
					m_Continuation.f_SetResult();
			}
		};
		
		template <typename t_CType>
		class TCActorResultVector
		{
		public:
			struct CQueuedResult
			{
				mint m_iResult;
				TCAsyncResult<t_CType> m_Result;
				CQueuedResult *m_pNext;
			};
			
			struct CInternal : public NPtr::TCSharedPointerIntrusiveBase<>
			{
				CInternal();
				~CInternal();
			public:
				friend class TCActorResultVector;
				
				align_cacheline NAtomic::TCAtomic<mint> mp_nAdded;
				align_cacheline NAtomic::TCAtomic<mint> mp_nFinished;
				align_cacheline NAtomic::TCAtomic<CQueuedResult *> mp_pFirstResult;
				NContainer::TCVector<TCAsyncResult<t_CType>> mp_Results;
				TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>> mp_GetResultsContinuation;
				bool mp_bDefinedSize = false;
				bool mp_bLazyResultsGotten = false;

				TCContinuation<NContainer::TCVector<TCAsyncResult<t_CType>>> f_GetResults();
				void fp_TransferResults();
			};

		public:
			NPtr::TCSharedPointer<CInternal> mp_pInternal;

			class CResultReceived
			{
				mint mp_iResult;
				NPtr::TCSharedPointer<CInternal> mp_pInternal;
			public:
				CResultReceived(mint _iResult, NPtr::TCSharedPointer<CInternal> const &_pInternal);
				void operator ()(TCAsyncResult<t_CType> &&_Result) const;
			};

		public:
			TCActorResultVector(mint _DefinedSize);
			TCActorResultVector();
			~TCActorResultVector();
			TCActorResultCall<TCActor<CAnyConcurrentActor>, CResultReceived> f_AddResult();
			auto f_GetResults();
			void f_SetLen(mint _DefinedSize);
		};

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
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
			)
		;

		template <typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void ()>>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult = []{}
			)
		;

		template <typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CType const &){}
			)
		;

		template <typename tf_FOnResult = NFunction::TCFunction<void ()>>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult = []{}
			)
		;

		template <typename t_CKey, typename t_CValue>
		class TCActorResultMap
		{
		public:

			class CInternalActor : public CActor
			{
				friend class TCActorResultMap;
				align_cacheline NAtomic::TCAtomic<smint> mp_nAdded;
				NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>> mp_Results;
				TCAutoClearInt<mint> mp_nFinished;
				TCContinuation<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> mp_GetResultsContinuation;
				TCAutoClearInt<bint> mp_bResultsGotten;

				TCContinuation<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> f_GetResults();
			public:
				enum
				{
					mc_bAllowInternalAccess = true
				};
			};

		private:

			static TCActor<CInternalActor> &fs_ActorType();
			TCActor<CInternalActor> mp_Actor;

			class CResultReceived
			{
				t_CKey m_Key;
				TCActor<CInternalActor> mp_Actor;
			public:
				template <typename tf_CKey>
				CResultReceived(tf_CKey &&_Key, TCActor<CInternalActor> const &_pActor);
				void operator ()(TCAsyncResult<t_CValue> &&_Result) const;
			};

		public:

			TCActorResultMap();

			template <typename tf_CKey>
			TCActorResultCall<TCActor<CInternalActor>, CResultReceived> f_AddResult(tf_CKey &&_Key);
			auto f_GetResults() -> decltype(fs_ActorType()(&CInternalActor::f_GetResults));
		};

		template <typename tf_CKey, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
		void fg_CombineResults
			(
				NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
			)
		;

		template <typename tf_CKey, typename tf_FOnResult = NFunction::TCFunction<void ()>>
		void fg_CombineResults
			(
				NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
			)
		;

		template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
			)
		;

		template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult = NFunction::TCFunction<void ()>>
		bool fg_CombineResults
			(
				TCContinuation<tf_CReturn> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
			)
		;

		template <typename tf_CKey, typename tf_CType, typename tf_FOnResult = NFunction::TCFunction<void (tf_CType const &)>>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &, tf_CType const &){}
			)
		;

		template <typename tf_CKey, typename tf_FOnResult = NFunction::TCFunction<void ()>>
		bool fg_CombineResults
			(
				TCContinuation<void> const &_Continuation
				, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
				, tf_FOnResult &&_fOnResult = [](tf_CKey const &){}
			)
		;
	}
}

#include "Malterlib_Concurrency_Utils.hpp"