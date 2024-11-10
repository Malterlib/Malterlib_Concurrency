// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_BoundActorCall.h"

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> TCFuture<t_CReturnValue>::f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit) &&
	{
		TCPromise<t_CReturnValue> Promise{CPromiseConstructNoConsume()};
		NStorage::TCSharedPointer<NAtomic::CAtomicFlag> pReplied = fg_Construct();
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[pReplied, Promise](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					if (!pReplied->f_TestAndSet())
						Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;

		fg_OneshotTimer
			(
				_Timeout
				, [pReplied, Promise, _TimeoutMessage]() -> NConcurrency::TCFuture<void>
				{
					if (!pReplied->f_TestAndSet())
						Promise.f_SetException(DMibErrorInstanceAsyncTimeout(_TimeoutMessage));
					co_return {};
				}
				, fg_DirectCallActor()
				, _bFireAtExit
			)
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	TCFuture<t_CReturn> TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_Timeout
		(
			fp64 _Timeout
			, NStr::CStr const &_TimeoutMessage
			, bool _bFireAtExit
		) &&
	{
		TCPromise<t_CReturn> Promise{CPromiseConstructNoConsume()};
		NStorage::TCSharedPointer<NAtomic::CAtomicFlag> pReplied = fg_Construct();
		fg_Move(*this).f_OnResultSet
			(
				[pReplied, Promise](TCAsyncResult<t_CReturn> &&_Result)
				{
					if (!pReplied->f_TestAndSet())
						Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;

		fg_OneshotTimer
			(
				_Timeout
				, [pReplied, Promise, _TimeoutMessage]() -> NConcurrency::TCFuture<void>
				{
					if (!pReplied->f_TestAndSet())
						Promise.f_SetException(DMibErrorInstanceAsyncTimeout(_TimeoutMessage));
					co_return {};
				}
				, fg_DirectCallActor()
				, _bFireAtExit
			)
		;

		return Promise.f_MoveFuture();
	}
}
