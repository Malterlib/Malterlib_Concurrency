// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void CDistributedActorTrustManager::CTrustTicket::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			_Stream << m_ServerAddress;
			_Stream << m_ServerPublicCert;
			_Stream << m_Token;
		}
		
		template <typename tf_CStream>
		void CDistributedActorTrustManager::CTrustTicket::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid trust ticket version");
			_Stream >> m_ServerAddress;
			_Stream >> m_ServerPublicCert;
			_Stream >> m_Token;
		}
		
		CDistributedActorTrustManager_Address const &CDistributedActorTrustManager::CAddressConnectionState::f_GetAddress() const
		{
			return NContainer::TCMap<CDistributedActorTrustManager_Address, CAddressConnectionState>::fs_GetKey(*this);
		}
		
		NStr::CStr const &CDistributedActorTrustManager::CHostConnectionState::f_GetHostID() const
		{
			return NContainer::TCMap<NStr::CStr, CHostConnectionState>::fs_GetKey(*this);
		}
	
		NStr::CStr const &CDistributedActorTrustManager::CNamespacePermissions::f_GetName() const
		{
			return NContainer::TCMap<NStr::CStr, CNamespacePermissions>::fs_GetKey(*this);
		}
		
		bool CDistributedActorTrustManager::CNamespacePermissions::operator ==(CNamespacePermissions const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_AllowedHosts, m_DisallowedHosts) == NContainer::fg_TupleReferences(_Right.m_AllowedHosts, _Right.m_DisallowedHosts); 
		}
		
		bool CDistributedActorTrustManager::CNamespacePermissions::operator <(CNamespacePermissions const &_Right) const
		{
			return NContainer::fg_TupleReferences(m_AllowedHosts, m_DisallowedHosts) < NContainer::fg_TupleReferences(_Right.m_AllowedHosts, _Right.m_DisallowedHosts); 
		}

		template <typename tf_CString>
		void CDistributedActorTrustManager::CNamespacePermissions::f_Format(tf_CString &o_String) const
		{
			o_String += typename tf_CString::CFormat("Allowed {vs} Disallowed {vs}") << m_AllowedHosts << m_DisallowedHosts;
		}

		template <typename t_CActor>
		TCTrustedActorSubscription<t_CActor>::TCTrustedActorSubscription(TCTrustedActorSubscription &&_Other)
			: mp_pState(fg_Move(_Other.mp_pState))
			, m_Actors(fg_Move(_Other.m_Actors))
		{
			if (mp_pState)
				mp_pState->m_pSubscription = this;
		}
		
		template <typename t_CActor>
		auto TCTrustedActorSubscription<t_CActor>::operator =(TCTrustedActorSubscription &&_Other) -> TCTrustedActorSubscription &
		{
			mp_pState = fg_Move(_Other.mp_pState); 
			m_Actors = fg_Move(_Other.m_Actors);
			if (mp_pState)
				mp_pState->m_pSubscription = this;
			return *this;
		}
		
		template <typename t_CActor>
		TCTrustedActorSubscription<t_CActor>::~TCTrustedActorSubscription()
		{
			f_Clear();
		}
		
		template <typename t_CActor>
		void TCTrustedActorSubscription<t_CActor>::f_Clear()
		{
			if (!mp_pState)
				return;
			auto &State = *mp_pState;
			auto TrustManager = State.m_TrustManager.f_Lock();
			if (TrustManager)
				TrustManager(&CDistributedActorTrustManager::fp_UnsubscribeTrustedActors, mp_pState) > fg_DiscardResult();
			State.m_pSubscription = nullptr;
			State.m_DispatchActor.f_Clear();
			State.m_fOnNewActor.f_Clear();
			State.m_fOnRemovedActor.f_Clear();
			mp_pState = nullptr;
			m_Actors.f_Clear();
		}

		template <typename t_CActor>
		TCContinuation<TCTrustedActorSubscription<t_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActors
			(
				NStr::CStr const &_Namespace
				, TCActor<CActor> const &_Actor
			)
		{
			NPtr::TCSharedPointer<typename TCTrustedActorSubscription<t_CActor>::CState> pState = fg_Construct();
			auto &State = *pState;
			State.m_DispatchActor = _Actor;
			State.m_TrustManager = fg_ThisActor(this);
			State.m_TypeHash = fg_GetTypeHash<t_CActor>();
			State.m_NamespaceName = _Namespace;
			
			TCContinuation<TCTrustedActorSubscription<t_CActor>> Continuation;
			
			fg_ThisActor(this)(&CDistributedActorTrustManager::fp_SubscribeTrustedActors, pState) > Continuation / [pState, Continuation]()
				{
					TCTrustedActorSubscription<t_CActor> Result;
					Result.mp_pState = pState;
					pState->m_pSubscription = &Result;
					
					if (pState->m_pActors)
					{
						auto &Actors = *pState->m_pActors;
						for (auto &Actor : Actors)
							Result.m_Actors[(TCDistributedActor<t_CActor> &)Actor];
					}
					Continuation.f_SetResult(fg_Move(Result));
				}
			;
			return Continuation;
		}

		template <typename t_CActor>
		void TCTrustedActorSubscription<t_CActor>::f_OnNewActor(NFunction::TCFunction<void (NFunction::CThisTag &, TCDistributedActor<t_CActor> const &_NewActor)> &&_fOnNewActor)
		{
			mp_pState->m_fOnNewActor = _fOnNewActor;
		}
		
		template <typename t_CActor>
		void TCTrustedActorSubscription<t_CActor>::f_OnRemoveActor(NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor)
		{
			mp_pState->m_fOnRemovedActor = _fOnRemovedActor;
		}
		
		template <typename t_CActor>
		void TCTrustedActorSubscription<t_CActor>::CState::f_AddDistributedActors(NContainer::TCSet<TCDistributedActor<CActor>> const &_Actors)
		{
			fg_Dispatch
				(
					m_DispatchActor
					, [this, _Actors, pThis = fg_Explicit(this)]
					{
						if (!m_pSubscription)
							return;

						for (auto &Actor : _Actors)
						{
							TCDistributedActor<t_CActor> &TypedActor = (TCDistributedActor<t_CActor> &)Actor; 
							
							auto &Subscription = *m_pSubscription;
							if (Subscription.m_Actors(TypedActor).f_WasCreated())
							{
								if (m_fOnNewActor)
									m_fOnNewActor(TypedActor);
							}
						}
					}
				)
				> fg_DiscardResult() 
			;
		}
		
		template <typename t_CActor>
		void TCTrustedActorSubscription<t_CActor>::CState::f_RemoveDistributedActors(NContainer::TCSet<TCWeakDistributedActor<CActor>> const &_Actors)
		{
			fg_Dispatch
				(
					m_DispatchActor
					, [this, _Actors, pThis = fg_Explicit(this)]
					{
						if (!m_pSubscription)
							return;
						
						for (auto &Actor : _Actors)
						{
							auto &Subscription = *m_pSubscription;
							if (Subscription.m_Actors.f_Remove((TCDistributedActor<t_CActor> &)Actor))
							{
								if (m_fOnRemovedActor)
									m_fOnRemovedActor(Actor);
							}
						}
					}
				)
				> fg_DiscardResult() 
			;
		}
	}
}
