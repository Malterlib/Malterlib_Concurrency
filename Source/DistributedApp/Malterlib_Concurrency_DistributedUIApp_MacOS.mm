// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedUIApp.h"

#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Concurrency/OSMainRunLoop>
#include <Mib/Concurrency/ActorHolderOSMainRunLoop>

#include <AppKit/NSApplication.h>

namespace NMib::NConcurrency
{
	void CDistributedUIApp::fsp_RunMain()
	{
		ch8	const *Dummy[] = {nullptr};
		NSApplicationMain(0, Dummy); // Will never return
	}
}
