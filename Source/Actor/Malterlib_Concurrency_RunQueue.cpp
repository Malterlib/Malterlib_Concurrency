// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	static_assert(sizeof(CConcurrentRunQueueEntry_Functor) == gc_ActorQueueDispatchFunctionMemory);
	static_assert
		(
			(sizeof(CConcurrentRunQueueEntry_Functor::m_Link) + sizeof(void *) + sizeof(CConcurrentRunQueueEntry_Functor::m_fToCall))
			== gc_ActorQueueDispatchFunctionMemory
		)
	;
	static_assert
		(
			sizeof(CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc)
			== fg_AlignUpConstExpr
			(
				sizeof(CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::m_Link)
				+ sizeof(CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::m_fToCall)
#if DMibConfig_Concurrency_LocalFirstScheduler && DMibConfig_Concurrency_LocalFirstDistribution
				+ sizeof(bool)
#endif
				, umint(sizeof(void *))
			)
		)
	;

	CConcurrentRunQueueEntry::CConcurrentRunQueueEntry()
	{
	}

	CConcurrentRunQueueEntry::~CConcurrentRunQueueEntry()
	{
	}

	void CConcurrentRunQueueEntry_Functor::f_Cleanup()
	{
		fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
	}

	void CConcurrentRunQueueEntry_Functor::f_Call(CConcurrencyThreadLocal &_ThreadLocal)
	{
		m_fToCall(_ThreadLocal);
		fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
	}

	CConcurrentRunQueueEntry_Functor::CConcurrentRunQueueEntry_Functor(FActorQueueDispatch &&_fToCall)
		: m_fToCall(fg_Move(_fToCall))
	{
	}

	CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc()
	{
	}

	CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::~CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc()
	{
	}

	CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc(FActorQueueDispatchNoAlloc &&_fToCall)
		: m_fToCall(fg_Move(_fToCall))
	{
	}

	void CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::f_Call(CConcurrencyThreadLocal &_ThreadLocal)
	{
		m_fToCall(_ThreadLocal);
		fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
	}

	void CConcurrentRunQueueEntry_FunctorNonVirtualNoAlloc::f_Cleanup()
	{
		fg_DeleteObjectDefiniteType(NMemory::CDefaultAllocator(), this);
	}
}
