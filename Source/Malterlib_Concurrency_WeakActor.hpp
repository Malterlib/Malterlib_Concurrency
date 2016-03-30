// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor()
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor)
			: m_pInternalActor(_pActor)
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor)
			: m_pInternalActor(fg_Move(_pActor))
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> const &_pActor)
			: m_pInternalActor(_pActor)
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> &&_pActor)
			: m_pInternalActor(fg_Move(_pActor))
		{
		}
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor const &_Other)
			: m_pInternalActor(_Other.m_pInternalActor)
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor &&_Other)
			: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
		{
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> const &_Other)
			: m_pInternalActor(_Other.m_pInternalActor)
		{
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> &&_Other)
			: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
		{
		}

		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> const &_Other)
			: m_pInternalActor(_Other.m_pInternalActor)
		{
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> &&_Other)
			: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor)
		{
			m_pInternalActor = _pActor;
			return *this;
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor)
		{
			m_pInternalActor = fg_Move(_pActor);
			return *this;
		}

		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> const &_pActor)
		{
			m_pInternalActor = _pActor;
			return *this;
		}

		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(NPtr::TCWeakPointer<CActorInternal, CInternalActorAllocator> &&_pActor)
		{
			m_pInternalActor = fg_Move(_pActor);
			return *this;
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor const &_Other)
		{
			m_pInternalActor = _Other.m_pInternalActor;
			return *this;
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor &&_Other)
		{
			m_pInternalActor = fg_Move(_Other.m_pInternalActor);
			return *this;
		}
		

		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor<tf_CActor> const &_Other)
		{
			m_pInternalActor = _Other.m_pInternalActor;
			return *this;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor<tf_CActor> &&_Other)
		{
			m_pInternalActor = fg_Move(_Other.m_pInternalActor);
			return *this;
		}	

		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> const &_Other)
		{
			m_pInternalActor = _Other.m_pInternalActor;
			return *this;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
		{
			m_pInternalActor = fg_Move(_Other.m_pInternalActor);
			return *this;
		}
		
		template <typename t_CActor>
		void TCWeakActor<t_CActor>::f_Clear()
		{
			m_pInternalActor.f_Clear();
		}
		
		template <typename t_CActor>
		bool TCWeakActor<t_CActor>::f_IsEmpty() const
		{
			return m_pInternalActor.f_IsEmpty();
		}
		
		template <typename t_CActor>
		TCActor<t_CActor> TCWeakActor<t_CActor>::f_Lock() const
		{
			TCActor<t_CActor> Ret;
			Ret.m_pInternalActor = m_pInternalActor.f_Lock();
			return Ret;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		bool TCWeakActor<t_CActor>::operator < (TCWeakActor<tf_CActor> const& _Right) const
		{
			return (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)m_pInternalActor 
				< (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)_Right.m_pInternalActor
			;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		bool TCWeakActor<t_CActor>::operator < (TCActor<tf_CActor> const& _Right) const
		{
			return (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)m_pInternalActor 
				< (NPtr::TCSharedPointer<CActorHolder, CInternalActorAllocator> const &)_Right.m_pInternalActor
			;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		bool TCWeakActor<t_CActor>::operator == (TCWeakActor<tf_CActor> const& _Right) const
		{
			return (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)m_pInternalActor 
				== (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)_Right.m_pInternalActor
			;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		bool TCWeakActor<t_CActor>::operator == (TCActor<tf_CActor> const& _Right) const
		{
			return (NPtr::TCWeakPointer<CActorHolder, CInternalActorAllocator> const &)m_pInternalActor 
				== (NPtr::TCSharedPointer<CActorHolder, CInternalActorAllocator> const &)_Right.m_pInternalActor
			;
		}

		template <typename t_CActor>
		inline_small TCWeakActor<t_CActor>::operator bool() const
		{
			return !m_pInternalActor.f_IsEmpty();
		}

	}
}
