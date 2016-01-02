// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CType, typename... tfp_CParams, typename... tfp_CHolderParams>
		TCActor<tf_CType> CConcurrencyManager::f_ConstructActor(TCConstruct<tf_CType, tfp_CParams...> &&_ConstructParams, tfp_CHolderParams&&... p_Params)
		{
			NPtr::TCUniquePointer<tf_CType> pRealActor = fg_Move(_ConstructParams);
			NPtr::TCSharedPointer<TCActorInternal<tf_CType>, NPtr::CSupportWeakTag, CInternalActorAllocator> pActor = fg_Construct(this, fg_Forward<tfp_CHolderParams>(p_Params)...);

			{
				DMibLock(m_ActorListLock);
				m_Actors.f_Insert(*pActor);
			}
			pRealActor->self.m_pThis = pActor.f_Get();
			pRealActor->m_pConcurrencyManager = this;
			pActor->mp_pActor = fg_Move(pRealActor);
			pActor->m_pConcurrencyManager = this;
			pActor->f_RefCountIncrease();
			
			// Handle exception in construct
			auto Cleanup = g_OnScopeExit > [&]
				{
					pActor->f_RefCountDecrease();
				}
			;
			pActor->fp_Construct();
			Cleanup.f_Clear(); // Disable refcount decrease
			
			return pActor;
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
						, fg_Forward<tfp_CHolderParams>(NContainer::fg_Get<tfp_Indidies>(_HolderParams.m_Params))...
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

	}
}
