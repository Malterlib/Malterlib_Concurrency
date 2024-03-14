// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool tf_bDirectCall>
	auto TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList, tf_bDirectCall>::f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit) &&
		-> TCFuture<CReturnType>
	{
		TCPromise<CReturnType> Promise;
		NStorage::TCSharedPointer<NAtomic::CAtomicFlag> pReplied = fg_Construct();
		fg_Move(*this) > fg_DirectResultActor() / [pReplied, Promise](TCAsyncResult<CReturnType> &&_Result)
			{
				if (!pReplied->f_TestAndSet())
					Promise.f_SetResult(fg_Move(_Result));
			}
		;

		fg_OneshotTimer
			(
				_Timeout
				, [pReplied, Promise, _TimeoutMessage]
				{
					if (!pReplied->f_TestAndSet())
						Promise.f_SetException(DMibErrorInstanceAsyncTimeout(_TimeoutMessage));
				}
				, fg_DirectCallActor()
				, _bFireAtExit
			)
		;

		return Promise.f_MoveFuture();
	}
}
