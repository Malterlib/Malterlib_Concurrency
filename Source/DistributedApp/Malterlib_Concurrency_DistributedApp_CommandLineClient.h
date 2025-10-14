// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib
#pragma once

#include <Mib/Encoding/EJson>
#include <Mib/Concurrency/ConcurrencyDefines>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorSingleSubscription>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/CommandLine/CommandLineClient>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppActor_Settings;

	struct CDistributedAppCommandLineClient : public NCommandLine::TCCommandLineClient<CCommandLineSpecificationDistributedAppCustomization, CDistributedAppCommandLineClient>
	{
		using FStopApp = NFunction::TCFunction<bool ()>;

		void f_MutateCommandLineSpecification
			(
				NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
			)
		;

		void f_SetLazyStartApp(NFunction::TCFunction<FStopApp (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyStartApp);
		void f_SetLazyPreRunDirectCommand(NFunction::TCFunction<void (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyPreRunDirectCommand);

		CDistributedAppCommandLineClient
			(
				CDistributedAppActor_Settings const &_Settings
				, NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
				, NContainer::TCMap<NStr::CStr, NStr::CStr> &&_TranslateHostnames
				, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
			)
		;
		~CDistributedAppCommandLineClient();

		CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other);
		CDistributedAppCommandLineClient &operator =(CDistributedAppCommandLineClient &&_Other);

	private:
		void fp_Init(NEncoding::CEJsonSorted const &_Params);

		friend struct NCommandLine::TCCommandLineClient<CCommandLineSpecificationDistributedAppCustomization, CDistributedAppCommandLineClient>;

		uint32 fp_RunCommand(void const *_pCommand, NEncoding::CEJsonSorted &&_Params);

		struct CInternal;

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
		NFunction::TCFunction<FStopApp (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> mp_fLazyStartApp;
		NFunction::TCFunction<void (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> mp_fLazyPreRunDirectCommand;
	};
}

namespace NMib::NCommandLine
{
	extern template struct NCommandLine::TCCommandLineClient<NConcurrency::CCommandLineSpecificationDistributedAppCustomization, NConcurrency::CDistributedAppCommandLineClient>;
}
