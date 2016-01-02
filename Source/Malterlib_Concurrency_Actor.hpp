// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		
		template <typename t_CActor>
		TCActor<t_CActor>::TCActor()
		{
		}

		template <typename t_CActor>
		TCActor<t_CActor>::TCActor(CNullPtr)
			: m_pInternalActor(nullptr)
		{
		}
		
		template <typename t_CActor>
		TCActor<t_CActor>::TCActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor)
			: m_pInternalActor(_pActor)
		{
		}
		template <typename t_CActor>
		TCActor<t_CActor>::TCActor(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor)
			: m_pInternalActor(fg_Move(_pActor))
		{
		}
		template <typename t_CActor>
		TCActor<t_CActor>::TCActor(TCActor const &_Other)
			: m_pInternalActor(_Other.m_pInternalActor)
		{
		}
		template <typename t_CActor>
		TCActor<t_CActor>::TCActor(TCActor &&_Other)
			: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
		{
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCActor<t_CActor>::TCActor(TCActor<tf_CActor> const &_Other)
			: m_pInternalActor(reinterpret_cast<TCActor const &>(_Other).m_pInternalActor)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		}
		template <typename t_CActor>
		template <typename tf_CActor>
		TCActor<t_CActor>::TCActor(TCActor<tf_CActor> &&_Other)
			: m_pInternalActor(fg_Move(reinterpret_cast<TCActor &>(_Other).m_pInternalActor))
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		}

		template <typename t_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> const &_pActor)
		{
			m_pInternalActor = _pActor;
			return *this;
		}
		template <typename t_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(NPtr::TCSharedPointer<CActorInternal, NPtr::CSupportWeakTag, CInternalActorAllocator> &&_pActor)
		{
			m_pInternalActor = fg_Move(_pActor);
			return *this;
		}
		template <typename t_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor const &_Other)
		{
			m_pInternalActor = _Other.m_pInternalActor;
			return *this;
		}
		template <typename t_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor &&_Other)
		{
			m_pInternalActor = fg_Move(_Other.m_pInternalActor);
			return *this;
		}
		
		template <typename t_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(CNullPtr)
		{
			m_pInternalActor = nullptr;
			return *this;
		}

		template <typename t_CActor>
		template <typename tf_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor<tf_CActor> const &_Other)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = reinterpret_cast<TCActor const &>(_Other).m_pInternalActor;
			return *this;
		}
		template <typename t_CActor>
		template <typename tf_CActor>
		TCActor<t_CActor> &TCActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = fg_Move(reinterpret_cast<TCActor &>(_Other).m_pInternalActor);
			return *this;
		}

		template <typename t_CActor>
		typename TCActor<t_CActor>::CActorInternal *TCActor<t_CActor>::operator ->() const
		{
			return &(*m_pInternalActor);
		}

		template <typename t_CActor>
		void TCActor<t_CActor>::f_Clear()
		{
			m_pInternalActor.f_Clear();
		}

		template <typename t_CActor>
		bool TCActor<t_CActor>::f_IsEmpty() const
		{
			return m_pInternalActor.f_IsEmpty();
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor> TCActor<t_CActor>::f_Weak() const
		{
			return *this;
		}
		
		template <typename t_CActor>
		bool TCActor<t_CActor>::operator < (TCActor const& _Right) const
		{
			return m_pInternalActor.f_Get() < _Right.m_pInternalActor.f_Get();
		}

		template <typename t_CActor>
		bool TCActor<t_CActor>::operator < (TCWeakActor<t_CActor> const& _Right) const
		{
			return m_pInternalActor.f_Get() < _Right.m_pInternalActor.f_Get();
		}
		
		template <typename t_CActor>
		TCActor<t_CActor>::operator bool() const
		{
			return !m_pInternalActor.f_IsEmpty();
		}
		
		template <typename t_CActor>
		template <typename tf_FResult>
		auto TCActor<t_CActor>::operator / (tf_FResult &&_fResult) const
			-> TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
		{
			return TCActorResultCall<TCActor, typename NTraits::TCRemoveReference<tf_FResult>::CType>
				(
					*this
					, fg_Forward<tf_FResult>(_fResult)
				)
			;
		}
		
		template <typename t_CActor>
		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto TCActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
			-> TCActorCall
			<
				TCActor
				, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
				, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
			>
		{
			return TCActorCall
				<
					TCActor
					, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
					, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
				>
				(
					*this
					, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
					, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
				)
			;
		}
		
		template <typename tf_CReturnType>
		tf_CReturnType CActor::f_DispatchWithReturn(NFunction::TCFunction<tf_CReturnType (NFunction::CThisTag &)> &&_fToDisptach)
		{
			return _fToDisptach();
		}
		
	}
}
