// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Encoding/JsonShortcuts>

#include "Malterlib_Concurrency_App_DDPBridgeHelper.h"

namespace NMib::NConcurrency::NDDPBridgeHelper
{
	void CDDPBridgeHelperActor::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		CDistributedAppActor::fp_BuildCommandLine(o_CommandLine);

		o_CommandLine.f_SetProgramDescription
			(
				"Malterlib DDP Bridge Helper"
				, "Sets up trust for use on a DDP client."
			)
		;

		auto DefaultSection = o_CommandLine.f_GetDefaultSection();
		(void)DefaultSection;
	}
}
