// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CActor = CActor>
		class TCWeakActor
		{
			friend class CConcurrencyManager;
			template <typename t_CActor2>
			friend class TCActor;
			template <typename t_CActor2>
			friend class TCWeakActor;
			
			friend class CConcurrencyManager;
		public:
			using CActorInternal = TCActorInternal<t_CActor>;
			using CContainedActor = t_CActor;
		private:
			NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> m_pInternalActor;
		public:
			TCWeakActor();
			TCWeakActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor);
			TCWeakActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor);
			TCWeakActor(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> const &_pActor);
			TCWeakActor(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> &&_pActor);
			TCWeakActor(TCWeakActor const &_Other);
			TCWeakActor(TCWeakActor &&_Other);
			
			template <typename tf_CActor>
			TCWeakActor(TCWeakActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCWeakActor(TCWeakActor<tf_CActor> &&_Other);
			template <typename tf_CActor>
			TCWeakActor(TCActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCWeakActor(TCActor<tf_CActor> &&_Other);
			
			TCWeakActor &operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor);
			TCWeakActor &operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor);
			TCWeakActor &operator =(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> const &_pActor);
			TCWeakActor &operator =(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> &&_pActor);
			TCWeakActor &operator =(TCWeakActor const &_Other);
			TCWeakActor &operator =(TCWeakActor &&_Other);

			template <typename tf_CActor>
			TCWeakActor &operator =(TCWeakActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCWeakActor &operator =(TCWeakActor<tf_CActor> &&_Other);
			template <typename tf_CActor>
			TCWeakActor &operator =(TCActor<tf_CActor> const &_Other);
			template <typename tf_CActor>
			TCWeakActor &operator =(TCActor<tf_CActor> &&_Other);
			
			void f_Clear();
			bool f_IsEmpty() const;
			
			TCActor<t_CActor> f_Lock() const;
			
			template <typename tf_CActor>
			bool operator < (TCWeakActor<tf_CActor> const& _Right) const;
			template <typename tf_CActor>
			bool operator < (TCActor<tf_CActor> const& _Right) const;
			
			template <typename tf_CActor>
			bool operator == (TCWeakActor<tf_CActor> const& _Right) const;
			template <typename tf_CActor>
			bool operator == (TCActor<tf_CActor> const& _Right) const;

			inline_small explicit operator bool() const;
			
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
			
			template <typename tf_CMemberFunction, typename... tfp_CCallParams>
			auto f_CallByValue(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
		};
	}
}

#include "Malterlib_Concurrency_WeakActor.hpp"
