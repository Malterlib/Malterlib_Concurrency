// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_ParallellForEach.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ParallellForEach(tf_CContainer &&_Container, tf_CFunctor &&_Functor)
		{
			if (_Container.f_IsEmpty())
				return;
			auto &ThreadPool = *g_ThreadPool;
			
			NThread::CEventAutoReset DoneEvent;
			align_cacheline NAtomic::TCAtomic<mint> nDispatched;
			
			NPtr::TCSharedPointer<NContainer::TCThreadSafeQueue<NFunction::TCFunction<void ()>>> pDispatchQueue = fg_Construct();
			
			NThread::CMutual ResultLock;
			NConcurrency::TCAsyncResult<void> Result;
			
			for (auto &Value : _Container)
			{
				auto *pValue = &Value;
				++nDispatched;
				pDispatchQueue->f_Push
					(
						[&, pValue]()
						{
							try
							{
								_Functor(*pValue);
							}
							catch (...)
							{
								DMibLock(ResultLock);
								if (!Result.f_IsSet())
									Result.f_SetCurrentException();
							}
							if (--nDispatched == 0)
								DoneEvent.f_Signal();
						}
					)
				;
				ThreadPool.f_Dispatch
					(
						[pDispatchQueue]()
						{
							while (auto ToDispatch = pDispatchQueue->f_Pop())
							{
								(*ToDispatch)();
							}
						}
					)
				;
			}
			
			while (auto ToDispatch = pDispatchQueue->f_PopLIFO())
				(*ToDispatch)();

			while (nDispatched.f_Load())
				DoneEvent.f_Wait();

#ifdef DMibContractConfigure_CheckEnabled
			mint Dispatched = nDispatched.f_Load();
			DMibCheck(Dispatched == 0)(Dispatched);
#endif
			
			if (Result.f_IsSet())
				Result.f_Get();
		}
		
	}
}
