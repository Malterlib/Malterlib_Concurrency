// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
	auto TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList>::f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit) -> TCDispatchedActorCall<CReturnType>
	{
		return fg_DirectDispatch
			(
				[_TimeoutMessage, _Timeout, _bFireAtExit, This = f_ByValue()]() mutable -> TCContinuation<CReturnType>
				{
					TCContinuation<CReturnType> Continuation;
					NPtr::TCSharedPointer<NAtomic::CAtomicFlag> pReplied = fg_Construct();
					This > [pReplied, Continuation](TCAsyncResult<CReturnType> &&_Result)
						{
							if (!pReplied->f_TestAndSet())
								Continuation.f_SetResult(fg_Move(_Result));
						}
					;
					
					fg_OneshotTimer
						(
							_Timeout
							, [pReplied, Continuation, _TimeoutMessage]
							{
								if (!pReplied->f_TestAndSet())
									Continuation.f_SetException(DMibErrorInstance(_TimeoutMessage));
							}
							, fg_DirectCallActor()
							, _bFireAtExit
						)
					;
					
					return Continuation;
				}
			)
		;
	}
}

namespace NMibOperators
{
	template <>
	class TCDisableAutomaticOperators<NMib::NConcurrency::CTimeoutHelper> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};
}
