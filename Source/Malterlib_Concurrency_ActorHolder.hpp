// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NConcurrency
	{

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> &&_ResultCall)
		{
			TCActor<CActor> pActor = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));

			pActor(&CActor::f_Destroy)
				> fg_ConcurrentActor() /  [pActor, _ResultCall](TCAsyncResult<void> &&_Result)
				{
					pActor->fp_Terminate();
					_ResultCall.mp_Actor->f_QueueProcess
						(
							[ResultCall = fg_Move(_ResultCall), Result = fg_Move(_Result)]() mutable
							{
								NPrivate::fg_CallResultFunctor(ResultCall.mp_Functor, ResultCall.mp_Actor->fp_GetActor(), fg_Move(Result));
							}
						)
					;
				}
			;
		}

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CActor &&_Actor, tf_CFunctor &&_Functor)
		{
			f_Destroy(fg_Forward<tf_CActor>(_Actor) / fg_Forward<tf_CFunctor>(_Functor));
		}

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CActor *_pActor, tf_CFunctor &&_Functor)
		{
			f_Destroy(fg_ThisActor(_pActor) / fg_Forward<tf_CFunctor>(_Functor));
		}

		template <typename tf_CFunctor>
		void CActorHolder::f_Destroy(tf_CFunctor &&_Functor)
		{
			auto pActor = mp_pConcurrencyManager->m_ThreadLocal->m_pCurrentActor;
			DMibFastCheck(pActor);
			f_Destroy(fg_ThisActor(pActor) / fg_Forward<tf_CFunctor>(_Functor));
		}
		
		CConcurrencyManager &CActorHolder::f_ConcurrencyManager() const
		{
			return *mp_pConcurrencyManager;
		}
		
		EPriority CActorHolder::f_GetPriority() const
		{
			return mp_Priority;
		}
	}
}
