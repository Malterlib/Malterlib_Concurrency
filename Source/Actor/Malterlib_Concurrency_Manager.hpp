// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	inline_always EPriority CConcurrencyManager::f_ClampPriority(EPriority _Priority) const
	{
		return _Priority < m_PriorityClamp ? m_PriorityClamp : _Priority;
	}

	template <typename tf_CActor>
	consteval bool CConcurrencyManager::fs_HasOverridenDestroy()
		requires (!NTraits::cIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>)
	{
		return true;
	}

	template <typename tf_CActor>
	consteval bool CConcurrencyManager::fs_HasOverridenDestroy()
		requires (NTraits::cIsSame<decltype(&CActor::fp_Destroy), decltype(&tf_CActor::fp_Destroy)>)
	{
		return false;
	}

	template <typename tf_CActor>
	consteval bool CConcurrencyManager::fs_HasOverridenDestroy()
	{
		return true;
	}

	namespace NPrivate
	{
		// The index is allocated on first use and identifies the type for as long as the process lives.
		// Windows builds compile without thread safe function local statics, so it is published by hand
		template <typename t_CActor>
		umint fg_GetSingletonIndex()
		{
			static constinit NAtomic::TCAtomic<umint> s_Index = 0; // Constant initialized, and holds the index plus one so that zero means unallocated

			umint Index = s_Index.f_Load(NAtomic::gc_MemoryOrder_Relaxed);

			if (!Index)
				Index = fg_AllocateSingletonIndex(s_Index);

			return Index - 1;
		}

	}

	template <typename t_CActor>
	CConcurrencyManager::TCSingleton<t_CActor>::TCSingleton(TCActor<t_CActor> &&_Actor)
		: m_Actor(fg_Move(_Actor))
	{
	}

	template <typename t_CActor>
	TCFuture<void> CConcurrencyManager::TCSingleton<t_CActor>::f_Destroy()
	{
		return fg_Move(m_Actor).f_Destroy();
	}

	// Creates the singleton actor on first use and keeps it until the concurrency manager starts
	// destroying, which destroys the singletons in reverse creation order before waiting for the
	// remaining actors. The create function runs under the singleton lock and may ask for other
	// singletons. The result is wrapped so that a caller awaiting it forwards the shutdown error
	// as its own result instead of catching an exception
	template <typename t_CActor, typename tf_FCreate>
	TCWrapped<TCActor<t_CActor>> CConcurrencyManager::f_GetSingleton(tf_FCreate const &_fCreate)
	{
		umint iSingleton = NPrivate::fg_GetSingletonIndex<t_CActor>();

		DMibLockTyped(NThread::CMutual, m_SingletonLock);

		if (m_bSingletonsDestroyed)
			return DMibErrorInstance("The concurrency manager is being destroyed and no longer creates singletons");

		if (iSingleton >= m_Singletons.f_GetLen())
			m_Singletons.f_SetLen(iSingleton + 1);

		auto &pSingleton = m_Singletons[iSingleton];

		if (pSingleton.f_IsEmpty())
		{
			pSingleton = fg_Construct<TCSingleton<t_CActor>>(_fCreate());
			m_SingletonCreationOrder.f_InsertLast(iSingleton);
		}

		return ((TCSingleton<t_CActor> *)pSingleton.f_Get())->m_Actor;
	}

	template <typename tf_CType, typename... tfp_CParams>
	TCActor<tf_CType> CConcurrencyManager::f_ConstructFromInternalActor
		(
			TCActorHolderSharedPointer<TCActorInternal<tf_CType>> &&_pInternalActor
			, TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams
		)
	{
		TCActorInternal<tf_CType> &InternalActor = *_pInternalActor.f_Get();

#if DMibConfig_Concurrency_DebugBlockDestroy
		InternalActor.m_ActorTypeName = fg_GetTypeName<tf_CType>();
#endif
		InternalActor.mp_bIsAlwaysAlive = TCIsActorAlwaysAlive<tf_CType>::mc_Value;
		InternalActor.mp_bHasOverriddenDestroy = fs_HasOverridenDestroy<tf_CType>();

		InternalActor.fp_ConstructActor
			(
				[&]
				{
					NMemory::TCAllocator_Placement<sizeof(tf_CType)> Allocator(InternalActor.m_ActorMemory);
					NStorage::TCUniquePointer<tf_CType, NMemory::TCAllocator_Placement<sizeof(tf_CType)>> pActorPlacement{fg_Move(_ConstructParams), fg_Move(Allocator)};

					// Construction was successful
					pActorPlacement.f_Detach();
				}
				, InternalActor.m_ActorMemory
			)
		;

		return fg_Move(_pInternalActor);
	}

	template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
	TCActor<tf_CType> CConcurrencyManager::f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params)
	{
		TCActorHolderSharedPointer<TCActorInternal<tf_CType>> pActor = fg_Construct(this, nullptr, fg_Forward<tfp_CHolderParams>(p_Params)...);

		auto Cleanup = g_OnScopeExit / [&pActor]
			{
				TCActor<tf_CType>(fg_Move(pActor)).f_Destroy().f_DiscardResult();
			}
		;

		auto pReturn = f_ConstructFromInternalActor<tf_CType>(fg_Move(pActor), fg_Move(_ConstructParams));

		Cleanup.f_Clear();

		return fg_Move(pReturn);
	}

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params)
	{
		return fg_ConcurrencyManager().f_ConstructActor(fg_Construct<tf_CActor>(fg_Forward<tfp_CParams>(p_Params)...));
	}

	namespace NPrivate
	{
		template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams, umint... tfp_Indidies>
		TCActor<tf_CActor> fg_ConstructActorHelper(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, NMeta::TCIndices<tfp_Indidies...> const&, tfp_CParams &&...p_Params)
		{
			return fg_ConcurrencyManager().f_ConstructActor
				(
					fg_Construct<tf_CActor>(fg_Forward<tfp_CParams>(p_Params)...)
					, fg_Forward<tfp_CHolderParams>(fg_Get<tfp_Indidies>(_HolderParams.m_Params))...
				)
			;
		}
	}

	template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params)
	{
		return NPrivate::fg_ConstructActorHelper<tf_CActor>
			(
				fg_Move(_HolderParams)
				, NMeta::TCConsecutiveIndices<sizeof...(tfp_CHolderParams)>()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	struct COnScopeExitActorHelperWithActor
	{
		COnScopeExitActorHelperWithActor(TCActor<CActor> const &_Actor)
			: m_Actor(_Actor)
		{
		}

		template <typename tf_FOnScopeExit>
		[[nodiscard]] COnScopeExitShared operator / (tf_FOnScopeExit &&_fOnExitFunctor) &&
		{
			return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>
				(
					[Actor = fg_Move(m_Actor), fOnExitFunctor = fg_Move(_fOnExitFunctor)]() mutable
					{
						fg_Dispatch
							(
								fg_Move(Actor)
								, fg_Move(fOnExitFunctor)
							)
							.f_DiscardResult()
						;
					}
				)
			;
		}

		TCActor<CActor> m_Actor;
	};

	struct COnScopeExitActorHelper
	{
		template <typename tf_FOnScopeExit>
		[[nodiscard]] COnScopeExitShared operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
		{
			DMibFastCheck(fg_CurrentActor());
			return fg_Construct<TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>>
				(
					[Actor = fg_CurrentActor(), fOnExitFunctor = fg_Move(_fOnExitFunctor)]() mutable
					{
						fg_Dispatch
							(
								fg_Move(Actor)
								, fg_Move(fOnExitFunctor)
							)
							.f_DiscardResult()
						;
					}
				)
			;
		}

		[[nodiscard]] COnScopeExitActorHelperWithActor operator () (TCActor<CActor> const &_Actor) const
		{
			return COnScopeExitActorHelperWithActor(_Actor);
		}
	};

	extern COnScopeExitActorHelper const &g_OnScopeExitActor;
}

namespace NMib
{
	extern template struct TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>;
}
