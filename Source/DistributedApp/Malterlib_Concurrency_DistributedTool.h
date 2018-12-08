// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	struct CDistributedToolAppActor;

	class CDistributedTool
	{
	public:
		virtual ~CDistributedTool() {}
		virtual void f_Register
			(
			 	TCActor<CDistributedToolAppActor> const &_ToolActor
			 	, CDistributedAppCommandLineSpecification::CSection &o_ToolsSection
			 	, CDistributedAppCommandLineSpecification &o_CommandLine
			 	, NStr::CStr const &_ClassName
			) = 0
		;
	};

	struct CDistributedToolSettings : public CDistributedAppActor_Settings
	{
		CDistributedToolSettings(NStr::CStr const &_ToolName);
	};

	struct CDistributedToolAppActor : public CDistributedAppActor
	{
		CDistributedToolAppActor(CDistributedToolSettings const &_Settings);
		CDistributedToolAppActor(CDistributedAppActor_Settings const &_Settings);
		~CDistributedToolAppActor();

	protected:

		struct CTool
		{
			NStorage::TCUniquePointer<CDistributedTool> m_pRuntimeClass;
			NStr::CStr m_ClassName;
		};

		void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine) override;
		TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) override;
		TCContinuation<void> fp_StopApp() override;

		NContainer::TCVector<CTool> mp_Tools;
	};
}
