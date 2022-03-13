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
		if (mp_pState)
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

	template <typename tf_CActor>
	auto CDistributedActorTrustManagerHolder::f_SubscribeTrustedActors(uint32 _MinSupportedVersion, uint32 _MaxSupportedVersion, TCActor<CActor> &&_Actor)
	{
		return this->template fp_GetAsActor<CDistributedActorTrustManager>().f_CallByValue<&CDistributedActorTrustManager::f_SubscribeTrustedActors<tf_CActor>>
			(
				tf_CActor::mc_pDefaultNamespace
				, fg_Move(_Actor)
				, _MinSupportedVersion
				, _MaxSupportedVersion
			)
		;
	}

	template <typename tf_CActor>
	TCFuture<TCTrustedActorSubscription<tf_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActors
		(
			NStr::CStr const &_Namespace
			, TCActor<CActor> const &_Actor
			, uint32 _MinSupportedVersion
			, uint32 _MaxSupportedVersion
		)
	{
		return f_SubscribeTrustedActorsWithVersion<tf_CActor>(_Namespace, _Actor, fg_SubscribeVersions<tf_CActor>(_MinSupportedVersion, _MaxSupportedVersion));
	}

	template <typename tf_CActor>
	TCFuture<TCTrustedActorSubscription<tf_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActorsWithVersion
		(
			NStr::CStr const &_Namespace
			, TCActor<CActor> const &_Actor
		 	, CDistributedActorProtocolVersions const &_Versions
		)
	{
		TCPromise<TCTrustedActorSubscription<tf_CActor>> Promise;

		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			return Promise <<= DMibErrorInstance("Invalid namespace name");

		if (!_Actor)
			return Promise <<= DMibErrorInstance("Invalid destination actor");

		NStorage::TCSharedPointer<typename TCTrustedActorSubscription<tf_CActor>::CState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TrustManager = fg_ThisActor(this);
		State.m_TypeHash = DMibConstantTypeHash(tf_CActor);
		State.m_ProtocolVersions = _Versions;
		State.m_NamespaceName = _Namespace;

		self(&CDistributedActorTrustManager::fp_SubscribeTrustedActors, pState)
			> Promise / [pState, Promise](NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> &&_Actors)
			{
				TCTrustedActorSubscription<tf_CActor> Result;
				Result.mp_pState = pState;
				pState->m_pSubscription = &Result;
				for (auto &Actor : _Actors)
				{
					auto &TrustedActor = Result.m_Actors[Actor.f_GetIdentifier()];
					TrustedActor.m_TrustInfo = Actor.m_TrustInfo;
					TrustedActor.m_ProtocolVersion = Actor.m_ProtocolVersion;
					TrustedActor.m_Actor = (TCDistributedActor<tf_CActor> &)Actor.m_Actor;
				}
				Promise.f_SetResult(fg_Move(Result));
			}
		;
		return Promise.f_MoveFuture();
	}

	template <typename t_CActor>
	TCFuture<void> TCTrustedActorSubscription<t_CActor>::f_OnActor
		(
			TCActorFunctor<TCFuture<void> (TCDistributedActor<t_CActor> const &_NewActor, CTrustedActorInfo const &_ActorInfo)> &&_fOnNewActor
			, NStr::CStr const &_ErrorCategory
			, NStr::CStr const &_ErrorPrefix
			, bool _bReportCurrent
		)
	{
		co_await ECoroutineFlag_AllowReferences;
		auto pState = mp_pState;

		pState->m_fOnNewActor = fg_Move(_fOnNewActor);
		pState->m_NewActorErrorCategory = _ErrorCategory;
		pState->m_NewActorErrorPrefix = _ErrorPrefix;

		if (!_bReportCurrent)
			co_return {};

		TCActorResultVector<void> Results;

		for (auto &Actor : m_Actors)
			pState->m_fOnNewActor(Actor.m_Actor, Actor.m_TrustInfo) > Results.f_AddResult();

		for (auto &Result : co_await Results.f_GetResults())
		{
			if (!Result)
			{
				DMibLogCategoryStr(pState->f_NewActorErrorCategory());
				DMibLog(Error, "{}: {}", pState->f_NewActorErrorPrefix(), Result.f_GetExceptionStr());
			}
		}

		co_return {};
	}

	template <typename t_CActor>
	void TCTrustedActorSubscription<t_CActor>::f_OnRemoveActor
		(
			TCActorFunctor<TCFuture<void> (TCWeakDistributedActor<CActor> const &_RemovedActor, CTrustedActorInfo &&_ActorInfo)> &&_fOnRemovedActor
			, NStr::CStr const &_ErrorCategory
			, NStr::CStr const &_ErrorPrefix
		)
	{
		auto &State = *mp_pState;

		State.m_fOnRemovedActor = fg_Move(_fOnRemovedActor);
		State.m_RemoveActorErrorCategory = _ErrorCategory;
		State.m_RemoveActorErrorPrefix = _ErrorPrefix;
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_NewActorErrorCategory() const
	{
		if (m_NewActorErrorCategory)
			return m_NewActorErrorCategory;

		return "Mib/Concurrency";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_NewActorErrorPrefix() const
	{
		if (m_NewActorErrorPrefix)
			return m_NewActorErrorPrefix;

		return "Failed to process on actor in actor subscription";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_RemoveActorErrorCategory() const
	{
		if (m_RemoveActorErrorCategory)
			return m_RemoveActorErrorCategory;

		return "Mib/Concurrency";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_RemoveActorErrorPrefix() const
	{
		if (m_RemoveActorErrorPrefix)
			return m_RemoveActorErrorPrefix;

		return "Failed to process on remove actor in actor subscription";
	}

	template <typename t_CActor>
	TCFuture<void> TCTrustedActorSubscription<t_CActor>::CState::f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> const &_Actors)
	{
		return g_Future <<= fg_Dispatch
			(
				m_DispatchActor
				, [this, _Actors, pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]() -> TCFuture<void>
				{
					if (!m_pSubscription)
						co_return {};

					TCActorResultVector<void> Results;

					for (auto &Actor : _Actors)
					{
						auto &Identifier = Actor.f_GetIdentifier();
						TCTrustedActor<t_CActor> &TypedActor = (TCTrustedActor<t_CActor> &)Actor;

						auto &Subscription = *m_pSubscription;
						if (Subscription.m_Actors(Identifier, TypedActor).f_WasCreated())
						{
							if (m_fOnNewActor)
								m_fOnNewActor(TypedActor.m_Actor, TypedActor.m_TrustInfo) > Results.f_AddResult();
						}
					}

					for (auto &Result : co_await Results.f_GetResults())
					{
						if (!Result)
						{
							DMibLogCategoryStr(pThis->f_NewActorErrorCategory());
							DMibLog(Error, "{}: {}", pThis->f_NewActorErrorPrefix(), Result.f_GetExceptionStr());
						}
					}

					co_return {};
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
				, [this, _Actors, pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]() -> TCFuture<void>
				{
					if (!m_pSubscription)
						co_return {};

					TCActorResultVector<void> Results;

					for (auto &Actor : _Actors)
					{
						auto &Subscription = *m_pSubscription;
						if (auto pActor = Subscription.m_Actors.f_FindEqual(Actor))
						{
							TCTrustedActor<t_CActor> &TypedActor = (TCTrustedActor<t_CActor> &)*pActor;
							auto TrustInfo = fg_Move(TypedActor.m_TrustInfo);
							auto Actor = pActor->m_Actor;
							Subscription.m_Actors.f_Remove(pActor);
							if (m_fOnRemovedActor)
								m_fOnRemovedActor(Actor.f_Weak(), fg_Move(TrustInfo)) > Results.f_AddResult();
						}
					}

					for (auto &Result : co_await Results.f_GetResults())
					{
						if (!Result)
						{
							DMibLogCategoryStr(pThis->f_RemoveActorErrorCategory());
							DMibLog(Error, "{}: {}", pThis->f_RemoveActorErrorPrefix(), Result.f_GetExceptionStr());
						}
					}

					co_return {};
				}
			)
		;
	}
}
