// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/OSMainRunLoop>

#import <AppKit/AppKit.h>

namespace NMib::NConcurrency::NPrivate
{
	bool fg_OSMainRunLoop_WaitApplicationEvent(fp64 _Timeout)
	{
		// Until the application object exists (the command line phase) only the run loop pumps. The
		// wait must return once any source is handled: that source may be the startup call that
		// creates the application, and the switch to event dequeuing only happens on re-entry
		if (!NSApp)
		{
			if (_Timeout < 0.0)
			{
				CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, true);

				return false;
			}

			return CFRunLoopRunInMode(kCFRunLoopDefaultMode, _Timeout.f_Get(), true) == kCFRunLoopRunTimedOut;
		}

		// Dequeuing an event runs the run loop internally (sources, timers and the main dispatch
		// queue included), and routing it gives windows their input; a wake stops the inner run
		// loop, which surfaces as a nil event
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
	// waking it requires posting one
	void fg_OSMainRunLoop_WakeApplication()
	{
		if (!NSApp)
			return;

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
