// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../DistributedActor/Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedTrust_Shared.h"
#include "Malterlib_Concurrency_DistributedTrust_Database.h"

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
		{
			if (mp_pState->m_DispatchActor == fg_CurrentActor())
			{
				mp_pState->m_pSubscription = this;
				DMibLogWithCategory
					(
						Mib/Concurrency/Distribution
						, SubscriptionLogVerbosity
						, "<{}, {}> Subscription valid in constructor (Apply deferred)"
						, mp_pState->m_NamespaceName
						, mp_pState->m_DebugID
					)
				;
				mp_pState->f_ApplyDeferredChanges();
			}
			else
			{
				DMibLogWithCategory
					(
						Mib/Concurrency/Distribution
						, SubscriptionLogVerbosity
						, "<{}, {}> Subscription not valid in constructor"
						, mp_pState->m_NamespaceName
						, mp_pState->m_DebugID
					)
				;
				mp_pState->m_pSubscription = nullptr;
			}
		}
		else
		{
			DMibLogWithCategory(Mib/Concurrency/Distribution, SubscriptionLogVerbosity, "State not valid in constructor");
		}
	}

	template <typename t_CActor>
	auto TCTrustedActorSubscription<t_CActor>::operator = (TCTrustedActorSubscription &&_Other) -> TCTrustedActorSubscription &
	{
		mp_pState = fg_Move(_Other.mp_pState);
		m_Actors = fg_Move(_Other.m_Actors);
		mp_pState->m_pSubscription = nullptr;
		if (mp_pState)
		{
			if (mp_pState->m_DispatchActor == fg_CurrentActor())
			{
				mp_pState->m_pSubscription = this;
				DMibLogWithCategory
					(
						Mib/Concurrency/Distribution
						, SubscriptionLogVerbosity
						, "<{}, {}> Subscription valid in operator = (Apply deferred)"
						, mp_pState->m_NamespaceName
						, mp_pState->m_DebugID
					)
				;
				mp_pState->f_ApplyDeferredChanges();
			}
			else
			{
				DMibLogWithCategory
					(
						Mib/Concurrency/Distribution
						, SubscriptionLogVerbosity
						, "<{}, {}> Subscription not valid in operator ="
						, mp_pState->m_NamespaceName
						, mp_pState->m_DebugID
					)
				;
				mp_pState->m_pSubscription = nullptr;
			}
		}
		else
		{
			DMibLogWithCategory(Mib/Concurrency/Distribution, SubscriptionLogVerbosity, "State not valid in operator =");
		}
		return *this;
	}

	template <typename t_CActor>
	TCTrustedActorSubscription<t_CActor>::~TCTrustedActorSubscription()
	{
		if (mp_pState)
			NPrivate::fg_HandleUndestroyedSubscription(f_Destroy());
	}

	template <typename t_CActor>
	TCUnsafeFuture<void> TCTrustedActorSubscription<t_CActor>::f_Destroy()
	{
		auto pState = fg_Move(mp_pState);

		if (!pState)
			co_return {};

		m_Actors.f_Clear();

		auto &State = *pState;

		State.m_pSubscription = nullptr;
		State.m_fOnNewActor.f_Clear();
		State.m_fOnRemovedActor.f_Clear();

		co_await fg_ContinueRunningOnActor(fg_ThisConcurrentActor());

		if (State.m_Subscription)
			co_await fg_Move(State.m_Subscription)->f_Destroy();

		co_return {};
	}

	template <typename t_CActor>
	bool TCTrustedActorSubscription<t_CActor>::f_IsEmpty() const
	{
		return mp_pState.f_IsEmpty();
	}

	template <typename t_CActor>
	bool TCTrustedActorSubscription<t_CActor>::f_HasActors() const
	{
		return !m_Actors.f_IsEmpty();
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
	TCTrustedActor<t_CActor>::TCTrustedActor(TCTrustedActor const &_Other)
		: m_Actor(_Other.m_Actor)
		, m_TrustInfo(_Other.m_TrustInfo)
		, m_ProtocolVersion(_Other.m_ProtocolVersion)
	{
	}

	template <typename tf_CActor>
	auto CDistributedActorTrustManagerHolder::f_SubscribeTrustedActors(uint32 _MinSupportedVersion, uint32 _MaxSupportedVersion, TCActor<CActor> &&_Actor)
	{
		return this->template fp_GetAsActor<CDistributedActorTrustManager>().f_Bind<&CDistributedActorTrustManager::f_SubscribeTrustedActors<tf_CActor>, EVirtualCall::mc_NotVirtual>
			(
				tf_CActor::mc_pDefaultNamespace
				, fg_Move(_Actor)
				, _MinSupportedVersion
				, _MaxSupportedVersion
			)
			.f_Call()
		;
	}

	template <typename tf_CActor>
	TCFuture<TCTrustedActorSubscription<tf_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActors
		(
			NStr::CStr _Namespace
			, TCActor<CActor> _Actor
			, uint32 _MinSupportedVersion
			, uint32 _MaxSupportedVersion
		)
	{
		return f_SubscribeTrustedActorsWithVersion<tf_CActor>(_Namespace, _Actor, fg_SubscribeVersions<tf_CActor>(_MinSupportedVersion, _MaxSupportedVersion));
	}

	template <typename tf_CActor>
	TCFuture<TCTrustedActorSubscription<tf_CActor>> CDistributedActorTrustManager::f_SubscribeTrustedActorsWithVersion
		(
			NStr::CStr _Namespace
			, TCActor<CActor> _Actor
			, CDistributedActorProtocolVersions _Versions
		)
	{
		if (!CActorDistributionManager::fs_IsValidNamespaceName(_Namespace))
			co_return DMibErrorInstance("Invalid namespace name");

		if (!_Actor)
			co_return DMibErrorInstance("Invalid destination actor");

		NStorage::TCSharedPointer<typename TCTrustedActorSubscription<tf_CActor>::CState> pState = fg_Construct();
		auto &State = *pState;
		State.m_DispatchActor = _Actor;
		State.m_TypeHash = DMibConstantTypeHash(tf_CActor);
		State.m_ProtocolVersions = _Versions;
		State.m_NamespaceName = _Namespace;
#if (DMibSysLogSeverities) & DMibConcurrency_SubscriptionLogVerbosity
		State.m_DebugID = NCryptography::fg_RandomID();
#endif

		auto Subscription = co_await fp_SubscribeTrustedActors(pState);

		TCTrustedActorSubscription<tf_CActor> Result;
		pState->m_Subscription = fg_Move(Subscription.m_Subscription);
		Result.mp_pState = pState;
		for (auto &Actor : Subscription.m_InitialActors)
		{
			auto &TrustedActor = Result.m_Actors[Actor.f_GetIdentifier()];
			TrustedActor.m_TrustInfo = Actor.m_TrustInfo;
			TrustedActor.m_ProtocolVersion = Actor.m_ProtocolVersion;
			TrustedActor.m_Actor = fg_StaticCast<TCDistributedActorWrapper<tf_CActor>>(Actor.m_Actor);
		}

		co_return fg_Move(Result);
	}

	template <typename t_CActor>
	TCUnsafeFuture<void> TCTrustedActorSubscription<t_CActor>::f_OnActor
		(
			TCActorFunctor<TCFuture<void> (TCDistributedActor<t_CActor> _NewActor, CTrustedActorInfo _ActorInfo)> &&_fOnNewActor
			, TCActorFunctor<TCFuture<void> (TCWeakDistributedActor<CActor> _RemovedActor, CTrustedActorInfo _ActorInfo)> &&_fOnRemovedActor
			, NStr::CStr const &_ErrorCategory
			, NStr::CStr const &_ErrorPrefix
		)
	{
		auto pState = mp_pState;

		auto &State = *pState;

		State.m_fOnNewActor = fg_Move(_fOnNewActor);
		State.m_fOnRemovedActor = fg_Move(_fOnRemovedActor);
		State.m_ErrorCategory = _ErrorCategory;
		State.m_ErrorPrefix = _ErrorPrefix;

		TCFutureVector<void> Results;

		if (pState->m_fOnNewActor)
		{
			for (auto &Actor : m_Actors)
			{
				TCPromiseFuturePair<void> OnNewActorFinishedPromise;
				DMibCheck(!Actor.mp_OnNewActorFinished.f_IsValid());
				Actor.mp_OnNewActorFinished = fg_Move(OnNewActorFinishedPromise.m_Future);

				fg_CallSafe
					(
						[pState, Actor = Actor.m_Actor, TrustInfo = Actor.m_TrustInfo, OnNewActorFinishedPromise = fg_Move(OnNewActorFinishedPromise.m_Promise)]() mutable -> TCFuture<void>
						{
							DMibFastCheck(pState->m_fOnNewActor);

							co_await pState->m_fOnNewActor(Actor, TrustInfo);

							OnNewActorFinishedPromise.f_SetResult();
							co_return {};
						}
					)
					> Results
				;
			}
		}
		else
			DMibLogWithCategory(Mib/Concurrency/Distribution, SubscriptionLogVerbosity, "<{}, {}> f_OnActor: No m_fOnNewActor", pState->m_NamespaceName, pState->m_DebugID);

		for (auto &Result : co_await fg_AllDoneWrapped(Results))
		{
			if (!Result)
			{
				DMibLogCategoryStr(State.f_NewActorErrorCategory());
				DMibLog(Error, "{}: {}", State.f_NewActorErrorPrefix(), Result.f_GetExceptionStr());
			}
		}

		co_return {};
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_NewActorErrorCategory() const
	{
		if (m_ErrorCategory)
			return m_ErrorCategory;

		return "Mib/Concurrency";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_NewActorErrorPrefix() const
	{
		if (m_ErrorPrefix)
			return NStr::CStr::CFormat(m_ErrorPrefix) << "on actor";

		return "Failed to process on actor in actor subscription";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_RemoveActorErrorCategory() const
	{
		if (m_ErrorCategory)
			return m_ErrorCategory;

		return "Mib/Concurrency";
	}

	template <typename t_CActor>
	NStr::CStr TCTrustedActorSubscription<t_CActor>::CState::f_RemoveActorErrorPrefix() const
	{
		if (m_ErrorPrefix)
			return NStr::CStr::CFormat(m_ErrorPrefix) << "on remove actor";

		return "Failed to process on remove actor in actor subscription";
	}

	template <typename t_CActor>
	FUnitVoidFutureFunction TCTrustedActorSubscription<t_CActor>::CState::f_AddDistributedActors(NContainer::TCMap<CDistributedActorIdentifier, TCTrustedActor<CActor>> &&_Actors)
	{
		return [this, Actors = fg_Move(_Actors), pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]() mutable -> TCFuture<void>
			{
				if (!m_pSubscription)
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/Distribution
							, SubscriptionLogVerbosity
							, "<{}, {}> f_AddDistributedActors: No subscription, deferred"
							, pThis->m_NamespaceName
							, pThis->m_DebugID
						)
					;

					m_DeferredChanges.f_Insert(fg_Move(Actors));
					co_return {};
				}

				TCFutureVector<void> Results;

				for (auto &Actor : Actors)
				{
					auto &Identifier = Actor.f_GetIdentifier();
					TCTrustedActor<t_CActor> &TypedActor = (TCTrustedActor<t_CActor> &)Actor;

					auto &Subscription = *m_pSubscription;
					auto MapResult = Subscription.m_Actors(Identifier, fg_Move(TypedActor));
					if (!MapResult.f_WasCreated())
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Distribution
								, SubscriptionLogVerbosity
								, "<{}, {}> f_AddDistributedActors: Was not created, ignored"
								, pThis->m_NamespaceName
								, pThis->m_DebugID
							)
						;
						continue;
					}

					auto &NewActor = *MapResult;

					if (!pThis->m_fOnNewActor)
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Distribution
								, SubscriptionLogVerbosity
								, "<{}, {}> f_AddDistributedActors: No m_fOnNewActor"
								, pThis->m_NamespaceName
								, pThis->m_DebugID
							)
						;
						continue;
					}

					TCPromiseFuturePair<void> OnNewActorFinishedPromise;
					DMibCheck(!NewActor.mp_OnNewActorFinished.f_IsValid());
					NewActor.mp_OnNewActorFinished = fg_Move(OnNewActorFinishedPromise.m_Future);

					fg_CallSafe
						(
							[
								pThis
								, Actor = NewActor.m_Actor
								, TrustInfo = NewActor.m_TrustInfo
								, OnNewActorFinishedPromise = fg_Move(OnNewActorFinishedPromise.m_Promise)
							]
							() mutable -> TCFuture<void>
							{
								DMibFastCheck(pThis->m_fOnNewActor);

								DMibLogWithCategory
									(
										Mib/Concurrency/Distribution
										, SubscriptionLogVerbosity
										, "<{}, {}> f_AddDistributedActors: Report new m_fOnNewActor {}"
										, pThis->m_NamespaceName
										, pThis->m_DebugID
										, TrustInfo.m_HostInfo
									)
								;

								if (pThis->m_fOnNewActor)
									co_await pThis->m_fOnNewActor(Actor, TrustInfo);

								OnNewActorFinishedPromise.f_SetResult();
								co_return {};
							}
						)
						> Results
					;
				}

				for (auto &Result : co_await fg_AllDoneWrapped(Results))
				{
					if (!Result)
					{
						DMibLogCategoryStr(pThis->f_NewActorErrorCategory());
						DMibLog(Error, "{}: {}", pThis->f_NewActorErrorPrefix(), Result.f_GetExceptionStr());
					}
				}

				co_return {};
			}
		;
	}

	template <typename t_CActor>
	FUnitVoidFutureFunction TCTrustedActorSubscription<t_CActor>::CState::f_RemoveDistributedActors(NContainer::TCSet<CDistributedActorIdentifier> &&_Actors)
	{
		return [this, Actors = _Actors, pThis = NStorage::TCSharedPointer<CState>{fg_Explicit(this)}]() mutable -> TCFuture<void>
			{
				if (!m_pSubscription)
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/Distribution
							, SubscriptionLogVerbosity
							, "<{}, {}> f_RemoveDistributedActors: No subscription, deferred"
							, pThis->m_NamespaceName
							, pThis->m_DebugID
						)
					;

					m_DeferredChanges.f_Insert(fg_Move(Actors));
					co_return {};
				}

				TCFutureVector<void> Results;

				for (auto &Actor : Actors)
				{
					auto &Subscription = *m_pSubscription;
					if (auto pActor = Subscription.m_Actors.f_FindEqual(Actor))
					{
						TCTrustedActor<t_CActor> &TypedActor = (TCTrustedActor<t_CActor> &)*pActor;
						auto TrustInfo = fg_Move(TypedActor.m_TrustInfo);
						auto Actor = pActor->m_Actor;
						auto OnNewActorFinished = fg_Move(pActor->mp_OnNewActorFinished);
						Subscription.m_Actors.f_Remove(pActor);

						fg_CallSafe
							(
								[pThis, WeakActor = Actor.f_Weak(), TrustInfo = fg_Move(TrustInfo), OnNewActorFinished = fg_Move(OnNewActorFinished)]() mutable
								-> TCFuture<void>
								{
									if (OnNewActorFinished.f_IsValid())
									{
										auto Result = co_await fg_Move(OnNewActorFinished).f_Wrap();
										if (!Result)
										{
											DMibLogCategoryStr(pThis->f_RemoveActorErrorCategory());
											DMibLog(Error, "{}: Failed to destroy on new actor finished: {}", pThis->f_RemoveActorErrorPrefix(), Result.f_GetExceptionStr());
										}
									}

									if (pThis->m_fOnRemovedActor)
									{
										DMibLogWithCategory
											(
												Mib/Concurrency/Distribution
												, SubscriptionLogVerbosity
												, "<{}, {}> f_RemoveDistributedActors: Report new m_fOnRemovedActor {}"
												, pThis->m_NamespaceName
												, pThis->m_DebugID
												, TrustInfo.m_HostInfo
											)
										;
										co_await pThis->m_fOnRemovedActor(fg_Move(WeakActor), fg_Move(TrustInfo));
									}
									else
									{
										DMibLogWithCategory
											(
												Mib/Concurrency/Distribution
												, SubscriptionLogVerbosity
												, "<{}, {}> f_RemoveDistributedActors: Report new m_fOnRemovedActor does not exist: {}"
												, pThis->m_NamespaceName
												, pThis->m_DebugID
												, TrustInfo.m_HostInfo
											)
										;
									}

									co_return {};
								}
							)
							> Results
						;
					}
					else
					{
						DMibLogWithCategory
							(
								Mib/Concurrency/Distribution
								, SubscriptionLogVerbosity
								, "<{}, {}> f_RemoveDistributedActors: No actor, ignored"
								, pThis->m_NamespaceName
								, pThis->m_DebugID
							)
						;
					}
				}

				for (auto &Result : co_await fg_AllDoneWrapped(Results))
				{
					if (!Result)
					{
						DMibLogCategoryStr(pThis->f_RemoveActorErrorCategory());
						DMibLog(Error, "{}: {}", pThis->f_RemoveActorErrorPrefix(), Result.f_GetExceptionStr());
					}
				}

				co_return {};
			}
		;
	}
}
