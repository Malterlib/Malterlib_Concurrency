// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedUIApp.h"

#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Concurrency/OSMainRunLoop>
#include <Mib/Concurrency/ActorHolderOSMainRunLoop>

#if defined(DPlatformFamily_OSX)
#include <AppKit/NSApplication.h>
#endif

namespace NMib::NConcurrency
{
	struct CDistributedUIApp::CInternal
	{
		struct CMainLoopActor : public CActor
		{
			using CActorHolder = COSMainRunLoopActorHolder;
		};

		~CInternal()
		{
			m_CurrentActor.f_Clear();
			m_HelperActor->f_BlockDestroy(m_pRunLoop->f_ActorDestroyLoop());
			m_HelperActor.f_Clear();
			while (m_pRunLoop->f_RefCountGet() > 0)
				m_pRunLoop->f_WaitOnceTimeout(0.1);
			m_pRunLoop.f_Clear();
		}

		NStorage::TCSharedPointer<COSMainRunLoop> m_pRunLoop = fg_Construct();
		TCActor<CMainLoopActor> m_HelperActor{fg_Construct()};
		NStorage::TCOptional<CCurrentlyProcessingActorScope> m_CurrentActor{fg_Construct(m_HelperActor)};
		CRunDistributedAppHelper m_RunHelper;
	};

	CDistributedUIApp::CDistributedUIApp(NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory)
		: mp_fActorFactory(_fActorFactory)
	{
	}

#if defined(DPlatformFamily_OSX)
#else
	CDistributedUIApp::fsp_RunMain()
	{
		DMibPDebugBreak; // Not implemented
	}
#endif

	CDistributedUIApp::~CDistributedUIApp() = default;


	NStorage::TCSharedPointer<CRunLoop> CDistributedUIApp::f_GetRunLoop() const
	{
		return mp_pInternal->m_pRunLoop;
	}

	void CDistributedUIApp::fp_AddCommands(CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
	{
		auto DefaultSection = o_CommandLine.f_GetDefaultSection();

		auto RunCommand = DefaultSection.f_RegisterDirectCommand
			(
				{
					"Names"_= {"--run-application"}
					, "Description"_= "Runs the application.\n"
					, "ErrorOnOptionAsParameter"_= false
					, "ErrorOnOptionAsParameterWhenDefaultCommand"_= false
					, "ErrorOnCommandAsParameter"_= false
					, "GreedyDefaultCommandParameters"_= true
					, "Parameters"_=
					{
						"AllParams...?"_=
						{
							"Type"_= {""}
							, "Default"_= _[_]
							, "Description"_= "Catch all parameters"
						}
					}
				}
				, [this](NEncoding::CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					mp_bRunAsApplication = true;
					return 0;
				}
				, EDistributedAppCommandFlag_RunLocalApp | EDistributedAppCommandFlag_WaitForRemotes
			)
		;

		if (!fg_GetSys()->f_GetEnvironmentVariable("TERM"))
			o_CommandLine.f_SetDefaultCommand(RunCommand);
	}

	void CDistributedUIApp::f_Stop()
	{
		if (!mp_pInternal)
			return;

		auto pInternal = fg_Move(mp_pInternal);
		pInternal->m_RunHelper.f_Stop();
	}

	aint CDistributedUIApp::f_Run()
	{
		mp_pInternal = fg_Construct();

		auto Return = mp_pInternal->m_RunHelper.f_RunCommandLine
			(
				mp_fActorFactory
				, [this](CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)
				{
					fp_AddCommands(o_CommandLine, _Settings);
				}
				, false
				, mp_pInternal->m_pRunLoop
			)
		;

		if (mp_bRunAsApplication)
			fsp_RunMain()
		else
			mp_pInternal->m_RunHelper.f_Stop();

		mp_pInternal.f_Clear();

		return Return;
	}
}
