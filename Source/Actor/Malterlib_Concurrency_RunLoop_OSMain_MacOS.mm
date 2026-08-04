// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/OSMainRunLoop>

#import <AppKit/AppKit.h>

extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *_pPool);

namespace NMib::NConcurrency::NPrivate
{
	// A negative timeout waits indefinitely; returns true on timeout. The caller holds the pass's
	// autorelease pool
	bool fg_OSMainRunLoop_WaitApplicationEvent(fp64 _Timeout)
	{
		// A handled source may create NSApp; return so the next wait can switch to event dequeuing.
		if (!NSApp)
		{
			if (_Timeout < 0.0)
			{
				CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, true);

				return false;
			}

			return CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), true) == kCFRunLoopRunTimedOut;
		}

		// Event dequeuing also pumps run-loop sources, timers and the main dispatch queue.
		NSDate *pUntil = _Timeout < 0.0 ? [NSDate distantFuture] : [NSDate dateWithTimeIntervalSinceNow:_Timeout.f_Get()];
		NSEvent *pEvent = [NSApp
			nextEventMatchingMask:NSEventMaskAny
			untilDate:pUntil
			inMode:NSDefaultRunLoopMode
			dequeue:YES]
		;

		if (pEvent)
		{
			[NSApp sendEvent:pEvent];

			return false;
		}

		return _Timeout >= 0.0 && pUntil.timeIntervalSinceNow <= 0.0;
	}

	// Event dequeuing only returns for events: run loop wakes are absorbed by the event wait, so
	// waking it requires posting one. Wakes come from any thread, including ones without a pool
	void fg_OSMainRunLoop_WakeApplication()
	{
		if (!NSApp)
			return;

		@autoreleasepool
		{
			NSEvent *pWakeEvent = [NSEvent
				otherEventWithType:NSEventTypeApplicationDefined
				location:NSZeroPoint
				modifierFlags:0
				timestamp:0.0
				windowNumber:0
				context:nil
				subtype:0
				data1:0
				data2:0]
			;
			[NSApp postEvent:pWakeEvent atStart:YES];
		}
	}

	void *fg_OSMainRunLoop_PushAutoreleasePool()
	{
		return objc_autoreleasePoolPush();
	}

	void fg_OSMainRunLoop_PopAutoreleasePool(void *_pPool)
	{
		objc_autoreleasePoolPop(_pPool);
	}
}
