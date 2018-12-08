// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	struct CDistributedDaemon
	{
		CDistributedDaemon
			(
				NStr::CStr const &_DaemonName
				, NStr::CStr const &_DaemonDisplayName
				, NStr::CStr const &_DaemonDescription
				, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			)
		;
		~CDistributedDaemon();

		aint f_Run();

		NStr::CStr const m_DaemonName;
		NStr::CStr const m_DaemonDisplayName;
		NStr::CStr const m_DaemonDescription;
		TCActor<CDistributedAppActor> m_AppActor;

	private:
		void fp_AddDaemonCommands(CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings);

		NFunction::TCFunction<TCActor<CDistributedAppActor> ()> mp_fActorFactory;
		CDistributedAppActor_Settings mp_Settings;
	};
}
