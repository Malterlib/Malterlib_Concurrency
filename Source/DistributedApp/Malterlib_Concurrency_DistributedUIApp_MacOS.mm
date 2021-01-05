// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedUIApp.h"

#include <Mib/Encoding/JSONShortcuts>
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
