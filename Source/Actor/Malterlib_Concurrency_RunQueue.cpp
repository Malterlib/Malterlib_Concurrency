// Copyright © 2024 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	static_assert(sizeof(CConcurrentRunQueueEntry_Functor) == gc_ActorQueueDispatchFunctionMemory);
	static_assert
		(
			(
				sizeof(CConcurrentRunQueueEntry_Functor::m_Link) + sizeof(void *) + sizeof(CConcurrentRunQueueEntry_Functor::m_fToCall)
			)
			== gc_ActorQueueDispatchFunctionMemory
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
