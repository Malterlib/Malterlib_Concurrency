// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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
