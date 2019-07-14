// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib
#pragma once

#include <Mib/Encoding/EJSON>
#include <Mib/Concurrency/ConcurrencyDefines>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorSingleSubscription>
#include <Mib/CommandLine/AnsiEncoding>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppActor_Settings;

	struct CDistributedAppCommandLineClient
	{
		CDistributedAppCommandLineSpecification::CParsedCommandLine f_ParseCommandLine(NContainer::TCVector<NStr::CStr> const &_Params);
		aint f_RunCommandLine(NContainer::TCVector<NStr::CStr> const &_CommandLine = fg_GetSys()->f_GetCommandLineArgs());
		aint f_RunCommand(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
		void f_MutateCommandLineSpecification
			(
				NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine
				, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
			)
		;

		void f_SetLazyStartApp(NFunction::TCFunction<void (NEncoding::CEJSON const &_Params, CDistributedAppCommandLineSpecification::ECommandFlag _Flags)> const &_fLazyStartApp);
		void f_SetLazyPreRunDirectCommand(NFunction::TCFunction<void (NEncoding::CEJSON const &_Params)> const &_fLazyPreRunDirectCommand);

		bool f_ColorEnabled() const;
		bool f_Color24BitEnabled() const;
		bool f_ColorLightBackground() const;

		NCommandLine::EAnsiEncodingFlag f_AnsiEncodingFlags() const;
		NCommandLine::CAnsiEncoding f_AnsiEncoding() const;
		uint32 f_CommandLineWidth() const;
		uint32 f_CommandLineHeight() const;

		CDistributedAppCommandLineClient
			(
				CDistributedAppActor_Settings const &_Settings
				, NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
				, NContainer::TCMap<NStr::CStr, NStr::CStr> &&_TranslateHostnames
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

		NStorage::TCUniquePointer<CInternal> mp_pInternal;
		NFunction::TCFunction<void (NEncoding::CEJSON const &_Params, CDistributedAppCommandLineSpecification::ECommandFlag _Flags)> mp_fLazyStartApp;
		NFunction::TCFunction<void (NEncoding::CEJSON const &_Params)> mp_fLazyPreRunDirectCommand;
	};
}
