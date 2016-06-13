// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib
#pragma once

#include <Mib/Encoding/EJSON>
#include <Mib/Concurrency/ConcurrencyDefines>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorSingleSubscription>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedAppActor_Settings;
		
		struct CDistributedAppCommandLineClient
		{
			aint f_RunCommandLine(NContainer::TCVector<NStr::CStr> const &_CommandLine = fg_GetSys()->f_GetCommandLineArgs());
			aint f_RunCommand(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
			void f_MutateCommandLineSpecification
				(
					NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine
					, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
				)
			;
			
			void f_SetLazyStartApp(NFunction::TCFunction<void ()> const &_fLazyStartApp);
			
			CDistributedAppCommandLineClient
				(
					CDistributedAppActor_Settings const &_Settings
					, NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
				)
			;
			~CDistributedAppCommandLineClient();
			
			CDistributedAppCommandLineClient(CDistributedAppCommandLineClient const &_Other) = delete;
			CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other);
			CDistributedAppCommandLineClient &operator =(CDistributedAppCommandLineClient const &_Other) = delete;
			CDistributedAppCommandLineClient &operator =(CDistributedAppCommandLineClient &&_Other);
		
		private:
			void fp_Init();
			
			struct CInternal;
			
			NPtr::TCUniquePointer<CInternal> mp_pInternal;
			NFunction::TCFunction<void ()> mp_fLazyStartApp;
		};
	}
}
