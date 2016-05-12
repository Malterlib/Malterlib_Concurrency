// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CReturn>
		void CDistributedActorTrustManager::CInternal::f_RunAfterInit(TCContinuation<tf_CReturn> const &_Continuation, NFunction::TCFunction<void (NFunction::CThisTag &)> &&_Function)
		{
			if (m_Initialize == EInitialize_Success)
			{
				_Function();
				return;				
			}
			else if (m_Initialize == EInitialize_Failure)
			{
				_Continuation.f_SetException(DMibErrorInstance(fg_Format("Trust manager failed to initialize: {}", m_InitializeError)));
				return;				
			}
			
			(*m_pInitOnce)() > [this, _Continuation, Function = fg_Move(_Function)](TCAsyncResult<void> &&_Result) mutable
				{
					if (_Result)
						Function();
					else
						_Continuation.f_SetException(DMibErrorInstance(fg_Format("Trust manager failed to initialize: {}", _Result.f_GetExceptionStr())));
				}
			;
		}
	}
}
