// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedApp_SettingsProperties.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppThreadLocal
	{
		CDistributedAppActor_SettingsProperties m_DefaultSettings;
	};

	CDistributedAppThreadLocal &fg_DistributedAppThreadLocal();
}

