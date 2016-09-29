// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			template <typename t_CType>
			struct TCIsContinuation;
		}
		
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
			: m_pInternalActor(reinterpret_cast<TCWeakActor const &>(_Other).m_pInternalActor)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCWeakActor<tf_CActor> &&_Other)
			: m_pInternalActor(fg_Move(reinterpret_cast<TCWeakActor &>(_Other).m_pInternalActor))
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		}

		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> const &_Other)
			: m_pInternalActor(reinterpret_cast<TCActor<t_CActor> const &>(_Other).m_pInternalActor)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
		}

		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> const &_Other)
			: m_pInternalActor(_Other.m_pInternalActor)
		{
		}
		
		template <typename t_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<t_CActor> &&_Other)
			: m_pInternalActor(fg_Move(_Other.m_pInternalActor))
		{
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor>::TCWeakActor(TCActor<tf_CActor> &&_Other)
			: m_pInternalActor(fg_Move(reinterpret_cast<TCActor<t_CActor> &>(_Other).m_pInternalActor))
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
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
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = reinterpret_cast<TCWeakActor const &>(_Other).m_pInternalActor;
			return *this;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCWeakActor<tf_CActor> &&_Other)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = fg_Move(reinterpret_cast<TCWeakActor &>(_Other).m_pInternalActor);
			return *this;
		}	

		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> const &_Other)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = reinterpret_cast<TCActor<t_CActor> const &>(_Other).m_pInternalActor;
			return *this;
		}
		
		template <typename t_CActor>
		template <typename tf_CActor>
		TCWeakActor<t_CActor> &TCWeakActor<t_CActor>::operator =(TCActor<tf_CActor> &&_Other)
		{
			static_assert(NPtr::NPrivate::TCIsValidConversion<t_CActor, tf_CActor, CInternalActorAllocator, CInternalActorAllocator>::mc_Value, "Not a valid conversion");
			m_pInternalActor = fg_Move(reinterpret_cast<TCActor<t_CActor> &>(_Other).m_pInternalActor);
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

		template <typename t_CActor>
		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto TCWeakActor<t_CActor>::operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
		{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith
					<
						typename NTraits::TCAddPointer<typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType>::CFunctionType>::CType
						, void (tfp_CCallParams...)
					>::mc_Value 
					, "Invalid params for function"
				)
			;
#endif
			DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);
			return TCActorCall
				<
					TCWeakActor
					, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
					, typename NTraits::TCRemoveReference<decltype(fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
					, NMeta::TCTypeList<tfp_CCallParams...>
				>
				(
					*this
					, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
					, fg_Construct(fg_Forward<tfp_CCallParams>(p_CallParams)...)
				)
			;
		}
		
		template <typename t_CActor>
		template <typename tf_CMemberFunction, typename... tfp_CCallParams>
		auto TCWeakActor<t_CActor>::f_CallByValue(tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const
		{
#ifdef DMibConcurrency_CheckFunctionCalls
			static_assert
				(
					NTraits::TCIsCallableWith
					<
						typename NTraits::TCAddPointer<typename NTraits::TCMemberFunctionPointerTraits<typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType>::CFunctionType>::CType
						, void (tfp_CCallParams...)
					>::mc_Value 
					, "Invalid params for function"
				)
			;
#endif
			DMibFastCheck(!f_IsEmpty() || t_CActor::mc_bCanBeEmpty);
			return TCActorCall
				<
					TCWeakActor
					, typename NTraits::TCRemoveReference<tf_CMemberFunction>::CType
					, typename NTraits::TCRemoveReference<decltype(NContainer::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...))>::CType
					, NMeta::TCTypeList<tfp_CCallParams...>
				>
				(
					*this
					, fg_Forward<tf_CMemberFunction>(_pMemberFunction)
					, NContainer::fg_Tuple(fg_Forward<tfp_CCallParams>(p_CallParams)...)
				)
			;
		}

		template
		<
			typename tf_FToDispatch
			, TCEnableIfType<!NPrivate::TCIsContinuation<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
			= nullptr
		>
		auto fg_Dispatch(TCWeakActor<> const &_Actor, tf_FToDispatch &&_fDispatch)
		{
			using CReturnType = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType;
			
			return _Actor.f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<CReturnType>>
					, NFunction::TCFunctionMovable<TCContinuation<CReturnType> ()>
					(
						[fDispatch = fg_Forward<tf_FToDispatch>(_fDispatch)]() mutable
						{
							return TCContinuation<CReturnType>::fs_RunProtected() > fg_Forward<tf_FToDispatch>(fDispatch);
						}
					)
				)
			;
		}

		template
		<
			typename tf_FToDispatch
			, TCEnableIfType<NPrivate::TCIsContinuation<typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType>::mc_Value> *
			= nullptr
		>
		auto fg_Dispatch(TCWeakActor<> const &_Actor, tf_FToDispatch &&_fDispatch)
		{
			using CReturnType = typename NTraits::TCIsCallableWith<typename NTraits::TCRemoveReference<tf_FToDispatch>::CType, void ()>::CReturnType;
			
			return _Actor.f_CallByValue
				(
					&CActor::f_DispatchWithReturn<CReturnType>
					, NFunction::TCFunctionMovable<CReturnType ()>(fg_Forward<tf_FToDispatch>(_fDispatch))
				)
			;
		}
	}
}
