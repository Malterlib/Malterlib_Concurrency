// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	namespace NConcurrency
	{

		template <typename tf_CActor, typename tf_CFunctor>
		void CActorHolder::f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> const &_ResultCall)
		{
			TCActor<CActor> pActor = NPtr::TCSharedPointer<TCActorInternal<CActor>, NPtr::CSupportWeakTag, CInternalActorAllocator>(fg_Explicit((TCActorInternal<CActor> *)this));

			pActor(&CActor::f_Destroy)
				> fg_ConcurrentActor() /  [pActor, _ResultCall](TCAsyncResult<void> &&_Result)
				{
					pActor->fp_Terminate();
					auto Result = fg_LambdaMove(_Result);
					auto ResultCall = fg_LambdaMove(_ResultCall);
					auto fCallResult
						= [ResultCall, Result]()
						{
							NPrivate::fg_CallResultFunctor((*ResultCall).mp_Functor, (*ResultCall).mp_Actor->fp_GetActor(), fg_Move(*Result));
						}
					;
					if (_ResultCall.mp_Actor->f_Concurrent())
						_ResultCall.mp_Actor->f_QueueProcessConcurrent(fg_Move(fCallResult));
					else
						_ResultCall.mp_Actor->f_QueueProcess(fg_Move(fCallResult));
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
		
	}
}
