// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Core/Application>
#include <Mib/Daemon/Daemon>
#include <Mib/Concurrency/DistributedApp>

#include "Malterlib_Concurrency_App_DDPBridgeHelper.h"

using namespace NMib;
using namespace NMib::NConcurrency::NDDPBridgeHelper;

class CDDPBridgeHelper : public CApplication
{
	aint f_Main()
	{
		return NConcurrency::fg_RunApp
			(
				[]
				{
					return fg_ConstructActor<CDDPBridgeHelperActor>();
				}
			)
		;
	}
};

DAppImplement(CDDPBridgeHelper);
