// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "../DistributedActor/Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib::NConcurrency
{
	CDistributedActorTrustManager_Address const &CDistributedActorTrustManager::CAddressConnectionState::f_GetAddress() const
	{
		return NContainer::TCMap<CDistributedActorTrustManager_Address, CAddressConnectionState>::fs_GetKey(*this);
	}

	NStr::CStr const &CDistributedActorTrustManager::CHostConnectionState::f_GetHostID() const
	{
		return NContainer::TCMap<NStr::CStr, CHostConnectionState>::fs_GetKey(*this);
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
		f_Destroy() > fg_DiscardResult();
	}

	template <typename t_CActor>
	TCFuture<void> TCTrustedActorSubscription<t_CActor>::f_Destroy()
	{
		co_await ECoroutineFlag_AllowReferences;

		auto pState = fg_Move(mp_pState);

		if (!pState)
			co_return {};

		m_Actors.f_Clear();

		auto &State = *pState;
		auto TrustManager = State.m_TrustManager.f_Lock();

		State.m_pSubscription = nullptr;
		State.m_fOnNewActor.f_Clear();
		State.m_fOnRemovedActor.f_Clear();

		if (!TrustManager)
			co_return {};

		co_await fg_ContinueRunningOnActor(TrustManager);

		co_await TrustManager(&CDistributedActorTrustManager::fp_UnsubscribeTrustedActors, fg_Move(pState));

		co_return {};
	}

	template <typename t_CActor>
	bool TCTrustedActorSubscription<t_CActor>::f_IsEmpty() const
	{
		return mp_pState.f_IsEmpty();
	}

	template <typename t_CActor>
	TCTrustedActor<t_CActor> const *TCTrustedActorSubscription<t_CActor>::f_GetOneActor(NStr::CStr const &_HostID, NStr::CStr &o_Error) const
	{
		TCTrustedActor<t_CActor> const *pMatched = nullptr;
		for (auto &TrustedActor : m_Actors)
		{
			if (!_HostID.f_IsEmpty() && TrustedActor.m_TrustInfo.m_HostInfo.m_HostID != _HostID)
				continue;
			if (pMatched)
			{
				o_Error = NStr::fg_Format
					(
						"Found more than one suitable. '{}' and '{}'"
						, pMatched->m_TrustInfo.m_HostInfo
						, TrustedActor.m_TrustInfo.m_HostInfo
					)
				;
				return nullptr;
			}
			pMatched = &TrustedActor;
		}

		if (!pMatched)
		{
			if (m_Actors.f_IsEmpty())
				o_Error = "No actors";
			else
				o_Error = "No suitable found";
		}

		return pMatched;
	}

	template <typename t_CActor>
	inline CDistributedActorIdentifier const &TCTrustedActor<t_CActor>::f_GetIdentifier() const
	{
		return NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<t_CActor>>::fs_GetKey(*this);
	}

	template <typename t_CActor>
	TCFuture<TCTrustedActorSubscription<t_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActors
		(
			NStr::CStr const &_Namespace
			, TCActor<CActor> const &_Actor
		)
	{
		return f_SubscribeTrustedActorsWithVersion<t_CActor>(_Namespace, _Actor, fg_SubscribeVersions<t_CActor>());
	}

	template <typename t_CActor>
	TCFuture<TCTrustedActorSubscription<t_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActorsWithVersion
		(
			NStr::CStr const &_Namespace
			, TCActor<CActor> const &_Actor
		 	, CDistributedActorProtocolVersions const &_Versions
		)
	{
		TCPromise<TCTrustedActorSubscription<t_CActor>> Promise;

		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return Promise <<= DMibErrorInstance("Invalid namespace name");

		if (!_Actor)
			return Promise <<= DMibErrorInstance("Invalid destination actor");

		NStorage::TCSharedPointer<typename TCTrustedActorSubscription<t_CActor>::CState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TrustManager = fg_ThisActor(this);
		State.m_TypeHash = DMibConstantTypeHash(t_CActor);
		State.m_ProtocolVersions = _Versions;
		State.m_NamespaceName = _Namespace;

		self(&CDistributedActorTrustManager::fp_SubscribeTrustedActors, pState)
			> Promise / [pState, Promise](NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> &&_Actors)
			{
				TCTrustedActorSubscription<t_CActor> Result;
				Result.mp_pState = pState;
				pState->m_pSubscription = &Result;
				for (auto &Actor : _Actors)
				{
					auto &TrustedActor = Result.m_Actors[Actor.f_GetIdentifier()];
					TrustedActor.m_TrustInfo = Actor.m_TrustInfo;
					TrustedActor.m_ProtocolVersion = Actor.m_ProtocolVersion;
					TrustedActor.m_Actor = (TCDistributedActor<t_CActor> &)Actor.m_Actor;
				}
				Promise.f_SetResult(fg_Move(Result));
			}
		;
		return Promise.f_MoveFuture();
	}

	template <typename t_CActor>
	void TCTrustedActorSubscription<t_CActor>::f_OnActor
		(
			NFunction::TCFunctionMovable<void (TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> &&_fOnNewActor
		 	, bool _bReportCurrent
		)
	{
		mp_pState->m_fOnNewActor = fg_Move(_fOnNewActor);

		if (!_bReportCurrent)
			return;

		for (auto &Actor : m_Actors)
			mp_pState->m_fOnNewActor(Actor.m_Actor, Actor.m_TrustInfo);
	}

	template <typename t_CActor>
	void TCTrustedActorSubscription<t_CActor>::f_OnRemoveActor
		(
			NFunction::TCFunctionMovable<void (TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor
		)
	{
		mp_pState->m_fOnRemovedActor = fg_Move(_fOnRemovedActor);
	}

	template <typename t_CActor>
	TCFuture<void> TCTrustedActorSubscription<t_CActor>::CState::f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors)
	{
		return g_Future <<= fg_Dispatch
			(
				m_DispatchActor
				, [this, _Actors, pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]
				{
					if (!m_pSubscription)
						return;

					for (auto &Actor : _Actors)
					{
						auto &Identifier = Actor.f_GetIdentifier();
						TCTrustedActor<t_CActor> &TypedActor = (TCTrustedActor<t_CActor> &)Actor;

						auto &Subscription = *m_pSubscription;
						if (Subscription.m_Actors(Identifier, TypedActor).f_WasCreated())
						{
							if (m_fOnNewActor)
								m_fOnNewActor(TypedActor.m_Actor, TypedActor.m_TrustInfo);
						}
					}
				}
			)
		;
	}

	template <typename t_CActor>
	TCFuture<void> TCTrustedActorSubscription<t_CActor>::CState::f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> const &_Actors)
	{
		return g_Future <<= fg_Dispatch
			(
				m_DispatchActor
				, [this, _Actors, pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]
				{
					if (!m_pSubscription)
						return;

					for (auto &Actor : _Actors)
					{
						auto &Subscription = *m_pSubscription;
						if (auto pActor = Subscription.m_Actors.f_FindEqual(Actor))
						{
							auto Actor = pActor->m_Actor;
							Subscription.m_Actors.f_Remove(pActor);
							if (m_fOnRemovedActor)
								m_fOnRemovedActor(Actor.f_Weak());
						}
					}
				}
			)
		;
	}
}
