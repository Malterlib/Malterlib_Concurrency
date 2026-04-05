// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_ParallellForEach.h"

namespace NMib::NConcurrency
{
	template <typename tf_CContainer, typename tf_CFunctor>
	void fg_ParallellForEach(tf_CContainer &&_Container, tf_CFunctor &&_Functor, CThreadPool &_ThreadPool)
	{
		if (_Container.f_IsEmpty())
			return;
		auto &ThreadPool = _ThreadPool;
		if (ThreadPool.f_IsSingleThreaded())
		{
			for (auto &Value : _Container)
				_Functor(Value);
			return;
		}

		NThread::CEventAutoReset DoneEvent;
		align_cacheline NAtomic::TCAtomic<umint> nDispatched;

		NStorage::TCSharedPointer<NContainer::TCThreadSafeQueue<NFunction::TCFunction<void ()>>> pDispatchQueue = fg_Construct();

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
		umint Dispatched = nDispatched.f_Load();
		DMibCheck(Dispatched == 0)(Dispatched);
#endif

		if (Result.f_IsSet())
			Result.f_Get();
	}
}
