// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	struct CDistributedUIApp
	{
		CDistributedUIApp(NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory);
		~CDistributedUIApp();

		aint f_Run();
		void f_Stop();

		NStorage::TCSharedPointer<CRunLoop> f_GetRunLoop() const;

	private:
		struct CInternal;

		void fp_AddCommands(CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings);

		NFunction::TCFunction<TCActor<CDistributedAppActor> ()> mp_fActorFactory;
		NStorage::TCUniquePointer<CInternal> mp_pInternal;
		bool mp_bRunAsApplication = false;
	};
}
