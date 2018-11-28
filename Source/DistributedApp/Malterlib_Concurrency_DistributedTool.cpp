// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTool.h"

namespace NMib::NConcurrency
{
	DMibRuntimeClassBaseNamed(CDistributedTool, NMib::NConcurrency::CDistributedTool);

	CDistributedToolSettings::CDistributedToolSettings(NStr::CStr const &_ToolName)
		: CDistributedAppActor_Settings
		{
			CDistributedAppActor_Settings{_ToolName}
			.f_WaitForRemotes(false)
		}
	{
	}

	CDistributedToolAppActor::CDistributedToolAppActor(CDistributedAppActor_Settings const &_Settings)
		: CDistributedAppActor(_Settings)
	{
		CRunTimeObjectInfo *pRuntimeTypeInfo = fg_GetRuntimeTypeInfo("NMib::NConcurrency::CDistributedTool");

		if (!pRuntimeTypeInfo)
			DMibError("No distributed tool found");

		pRuntimeTypeInfo->f_ForEachLeafChild
			(
				[&](CRunTimeObjectInfo const &_RuntimeObjectInfo)
			 	{
					mp_Tools.f_Insert
						(
							{
								fg_Explicit((CDistributedTool *)_RuntimeObjectInfo.f_CreateObject())
								, _RuntimeObjectInfo.m_pName
							}
						)
					;
				}
		 	)
		;
	}

	CDistributedToolAppActor::CDistributedToolAppActor(CDistributedToolSettings const &_Settings)
		: CDistributedToolAppActor(static_cast<CDistributedAppActor_Settings const &>(_Settings))
	{
	}

	CDistributedToolAppActor::~CDistributedToolAppActor() = default;

	void CDistributedToolAppActor::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		CDistributedAppActor::fp_BuildCommandLine(o_CommandLine);

		auto ToolsSection = o_CommandLine.f_AddSection("Tools", "");

		auto ThisActor = fg_ThisActor(this);

		for (auto &Tool : mp_Tools)
			Tool.m_pRuntimeClass->f_Register(ThisActor, ToolsSection, o_CommandLine, Tool.m_ClassName);
	}

	TCContinuation<void> CDistributedToolAppActor::fp_StartApp(NEncoding::CEJSON const &_Params)
	{
		return fg_Explicit();
	}

	TCContinuation<void> CDistributedToolAppActor::fp_StopApp()
	{
		return fg_Explicit();
	}
}
