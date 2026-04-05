// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_DistributedApp_SettingsProperties.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppThreadLocal
	{
		CDistributedAppActor_SettingsProperties m_DefaultSettings;
		EDistributedAppType m_DefaultAppType = EDistributedAppType_Unknown;
		bool m_bRemoteCommandLine = false;
		bool m_bRemoteCommandLineConfigure = false;
	};

	CDistributedAppThreadLocal &fg_DistributedAppThreadLocal();
}

