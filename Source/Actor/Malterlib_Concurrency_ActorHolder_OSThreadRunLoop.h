// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#if defined(DPlatformFamily_macOS)
#include <IOKit/IOKitLib.h>
#endif

namespace NMib::NConcurrency
{
	class COSThreadRunLoopActorHolder : public CDefaultActorHolder
	{
	public:
		COSThreadRunLoopActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, NStr::CStr const &_ThreadName
			)
		;
		~COSThreadRunLoopActorHolder();

#if defined(DPlatformFamily_macOS)
		CFRunLoopRef const &f_GetRunLoop() const;
#endif

	protected:
		void fp_StartQueueProcessing() override;
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;

		NThread::CLowLevelLock mp_ThreadLock;
		NThread::CLowLevelLock mp_RunLoopDestroyLock;
		NStorage::TCUniquePointer<NThread::CThreadObject> mp_pThread;
		NThread::CEvent mp_ThreadStartedEvent;
		NStr::CStr mp_ThreadName;
#if defined(DPlatformFamily_macOS)
		CFRunLoopRef mp_RunLoopRef = nullptr;
		CFRunLoopSourceRef mp_RunLoopSourceRef = nullptr;
#endif
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
