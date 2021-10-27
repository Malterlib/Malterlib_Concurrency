// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
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
		InternalActor.fp_ConstructActor
			(
				[&]
				{
					NMemory::TCAllocator_Placement<sizeof(tf_CType)> Allocator(InternalActor.m_ActorMemory.m_Aligned);
					NStorage::TCUniquePointer<tf_CType, NMemory::TCAllocator_Placement<sizeof(tf_CType)>> pActorPlacement{fg_Move(_ConstructParams), Allocator};

					// Construction was successful
					pActorPlacement.f_Detach();
				}
				, InternalActor.m_ActorMemory.m_Aligned
			)
		;

		return fg_Move(_pInternalActor);
	}

	template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
	TCActor<tf_CType> CConcurrencyManager::f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params)
	{
		TCActorHolderSharedPointer<TCActorInternal<tf_CType>> pActor = fg_Construct(this, nullptr, fg_Forward<tfp_CHolderParams>(p_Params)...);

		return f_ConstructFromInternalActor<tf_CType>(fg_Move(pActor), fg_Move(_ConstructParams));
	}

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<tf_CActor> fg_ConstructActor(tfp_CParams &&...p_Params)
	{
		return fg_ConcurrencyManager().f_ConstructActor(fg_Construct<tf_CActor>(fg_Forward<tfp_CParams>(p_Params)...));
	}

	namespace NPrivate
	{
		template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams, mint... tfp_Indidies>
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
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CHolderParams)>::CType()
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
		COnScopeExitShared operator > (tf_FOnScopeExit &&_fOnExitFunctor) &&
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
							> fg_DiscardResult()
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
		COnScopeExitShared operator > (tf_FOnScopeExit &&_fOnExitFunctor) const
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
							> fg_DiscardResult()
						;
					}
				)
			;
		}

		template <typename tf_FOnScopeExit>
		COnScopeExitShared operator / (tf_FOnScopeExit &&_fOnExitFunctor) const
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
							> fg_DiscardResult()
						;
					}
				)
			;
		}

		COnScopeExitActorHelperWithActor operator () (TCActor<CActor> const &_Actor) const
		{
			return COnScopeExitActorHelperWithActor(_Actor);
		}
	};

	extern COnScopeExitActorHelper const &g_OnScopeExitActor;
}

namespace NMib
{
	extern template class TCOnScopeExit<NFunction::TCFunctionMovable<void ()>>;
}
