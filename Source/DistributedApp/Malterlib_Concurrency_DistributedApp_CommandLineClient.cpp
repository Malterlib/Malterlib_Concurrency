// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Process/StdInActor>

namespace NMib::NConcurrency
{
	struct CDistributedAppCommandLineClient::CInternal
	{
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<TCDistributedActorSingleSubscription<ICCommandLine>> m_CommandLineSubscription;
		NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> m_pCommandLineSpecification;
		CDistributedAppActor_Settings m_Settings;
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;
		bool m_bInitialized = false;
		bool m_bColorEnabled = true;
	};

	void CDistributedAppCommandLineClient::f_SetLazyStartApp
		(
		 	NFunction::TCFunction<void (NEncoding::CEJSON const &_Params, CDistributedAppCommandLineSpecification::ECommandFlag _Flags)> const &_fLazyStartApp
		)
	{
		mp_fLazyStartApp = _fLazyStartApp;
	}

	void CDistributedAppCommandLineClient::f_SetLazyPreRunDirectCommand(NFunction::TCFunction<void (NEncoding::CEJSON const &_Params)> const &_fLazyPreRunDirectCommand)
	{
		mp_fLazyPreRunDirectCommand = _fLazyPreRunDirectCommand;
	}

	aint CDistributedAppCommandLineClient::f_RunCommandLine(NContainer::TCVector<NStr::CStr> const &_CommandLine)
	{
		auto &Internal = *mp_pInternal;
		NException::CDisableExceptionTraceScope DisableTrace;
		auto ParsedCommandLine = Internal.m_pCommandLineSpecification->f_ParseCommandLine(_CommandLine);
		return f_RunCommand(ParsedCommandLine.m_Command, ParsedCommandLine.m_Params);
	}

	namespace
	{
		struct CCommandLineControlActor : public ICCommandLineControl
		{
			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				fp_CreateInputActor();

				TCPromise<TCActorSubscriptionWithID<>> Promise;
				m_InputActor(&NProcess::CStdInActor::f_RegisterForInputBinary, fg_Move(_fOnInput), _Flags) > Promise / [=](NConcurrency::CActorSubscription &&_Subscription)
					{
						Promise.f_SetResult(fg_Move(_Subscription));
					}
				;

				return Promise.f_MoveFuture();
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				fp_CreateInputActor();

				TCPromise<TCActorSubscriptionWithID<>> Promise;
				m_InputActor(&NProcess::CStdInActor::f_RegisterForInput, fg_Move(_fOnInput), _Flags) > Promise / [=](NConcurrency::CActorSubscription &&_Subscription)
					{
						Promise.f_SetResult(fg_Move(_Subscription));
					}
				;

				return Promise.f_MoveFuture();
			}

			TCFuture<NContainer::CSecureByteVector> f_ReadBinary() override
			{
				fp_CreateInputActor();
				return m_InputActor(&NProcess::CStdInActor::f_ReadBinary);
			}

			TCFuture<NStr::CStrSecure> f_ReadLine() override
			{
				fp_CreateInputActor();
				return m_InputActor(&NProcess::CStdInActor::f_ReadLine);
			}

			TCFuture<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) override
			{
				fp_CreateInputActor();
				return m_InputActor(&NProcess::CStdInActor::f_ReadPrompt, _Params);
			}

			TCFuture<void> f_AbortReads() override
			{
				fp_CreateInputActor();
				return m_InputActor(&NProcess::CStdInActor::f_AbortReads);
			}

			TCFuture<void> f_StdOut(NStr::CStrSecure const &_Output) override
			{
				DMibConOutRaw(_Output);
				return fg_Explicit();
			}

			TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) override
			{
				NSys::fg_ConsoleOutputBinary(_Output);
				return fg_Explicit();
			}

			TCFuture<void> f_StdErr(NStr::CStrSecure const &_Output) override
			{
				DMibConErrOutRaw(_Output);
				return fg_Explicit();
			}

		private:
			TCActor<NProcess::CStdInActor> m_InputActor;

			TCFuture<void> fp_Destroy() override
			{
				if (!m_InputActor)
					return fg_Explicit();
				return m_InputActor->f_Destroy();
			}

			void fp_CreateInputActor()
			{
				if (!m_InputActor)
					m_InputActor = fg_Construct();
			}
		};
	}

	bool CDistributedAppCommandLineClient::f_ColorEnabled() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.m_bColorEnabled;
	}

	aint CDistributedAppCommandLineClient::f_RunCommand(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		auto &Internal = *mp_pInternal;
		auto &CommandLineSpec = *(Internal.m_pCommandLineSpecification->mp_pInternal);

		Internal.m_bColorEnabled = _Params.f_GetMemberValue("Color", CDistributedAppActor::fs_ColorEnabledDefault()).f_Boolean();

		enum EHelpCommand
		{
			EHelpCommand_None
			, EHelpCommand_Normal
			, EHelpCommand_Verbose
		};

		auto fCommandHelp = [&]() -> EHelpCommand
			{
				if (auto pValue = _Params.f_GetMember("HelpCurrentCommandVerbose"))
				{
					if (pValue->f_Boolean())
						return EHelpCommand_Verbose;
				}
				if (auto pValue = _Params.f_GetMember("HelpCurrentCommand"))
				{
					if (pValue->f_Boolean())
						return EHelpCommand_Normal;
				}
				return EHelpCommand_None;
			}
		;

		if (auto HelpCommand = fCommandHelp())
		{
			TCVector<CStr> Params = {CFile::fs_GetFile(CFile::fs_GetProgramPath()), "--help", Internal.m_bColorEnabled ? "--color" : "--no-color", _Command};

			if (HelpCommand == EHelpCommand_Verbose)
				Params.f_Insert("-v");

			return f_RunCommandLine(Params);
		}

		auto pFoundCommand = CommandLineSpec.m_CommandByName.f_FindEqual(_Command);
		if (!pFoundCommand)
			DMibError(fg_Format("Command not found: {}", pFoundCommand));

		auto &Command = **pFoundCommand;

		if (Command.m_fDirectRunCommand)
		{
			if (mp_fLazyPreRunDirectCommand)
				mp_fLazyPreRunDirectCommand(_Params);

			return Command.m_fDirectRunCommand(_Params, *this);
		}
		else if (Command.m_fActorRunCommand)
		{
			if (mp_fLazyStartApp)
				mp_fLazyStartApp(_Params, Command.m_Flags);
			fp_Init();

			TCDistributedActor<CCommandLineControlActor> pCommandLineControl = Internal.m_DistributionManager->f_ConstructActor<CCommandLineControlActor>();

			auto CommandLineActor = Internal.m_CommandLineSubscription(&TCDistributedActorSingleSubscription<ICCommandLine>::f_GetActor).f_CallSync(10.0);

			CCommandLineControl CommandLineControl;
			CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

			auto ConsoleProperties = NSys::fg_GetConsoleProperties();
			CommandLineControl.m_CommandLineWidth = ConsoleProperties.m_Width;
			CommandLineControl.m_CommandLineHeight = ConsoleProperties.m_Height;
			CommandLineControl.m_bColorEnabled = Internal.m_bColorEnabled;

			aint Status = DMibCallActor
				(
					 CommandLineActor
					 , ICCommandLine::f_RunCommandLine
					 , Command.m_Names.f_GetFirst()
					 , _Params
					 , fg_Move(CommandLineControl)
				).f_CallSync()
			;

			pCommandLineControl->f_Destroy().f_CallSync();

			return Status;
		}
		return 0;
	}

	void CDistributedAppCommandLineClient::f_MutateCommandLineSpecification
		(
			NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
		)
	{
		auto &Internal = *mp_pInternal;
		_fMutate(*Internal.m_pCommandLineSpecification, Internal.m_Settings);
	}

	CDistributedAppCommandLineClient::CDistributedAppCommandLineClient
		(
			CDistributedAppActor_Settings const &_Settings
			, NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
			, NContainer::TCMap<NStr::CStr, NStr::CStr> &&_TranslateHostnames
		)
		: mp_pInternal(fg_Construct())
	{
		auto &Internal = *mp_pInternal;
		Internal.m_pCommandLineSpecification = _pCommandLineSpecification;
		Internal.m_Settings = _Settings;
		Internal.m_TranslateHostnames = fg_Move(_TranslateHostnames);
	}

	CDistributedAppCommandLineClient::~CDistributedAppCommandLineClient()
	{
		if (mp_pInternal && mp_pInternal->m_TrustManager)
			mp_pInternal->m_TrustManager->f_BlockDestroy();
	}

	class CDistributedActorTrustManagerDatabase_JSONDirectory_CommandLine : public CDistributedActorTrustManagerDatabase_JSONDirectory
	{
		CDistributedActorTrustManagerDatabase_JSONDirectory_CommandLine(CStr const &_BaseDirectory, CStr const &_Enclave)
			: CDistributedActorTrustManagerDatabase_JSONDirectory(_BaseDirectory)
		{
		}
	private:
	};

	void CDistributedAppCommandLineClient::fp_Init()
	{
		auto &Internal = *mp_pInternal;
		if (!Internal.m_bInitialized)
		{
			CDistributedActorTrustManager::COptions Options;

			Options.m_fConstructManager = [](CActorDistributionManagerInitSettings const &_Settings) -> TCActor<CActorDistributionManager>
				{
					return fg_ConstructActor<CActorDistributionManager>(_Settings);
				}
			;
			Options.m_KeySetting = Internal.m_Settings.m_KeySetting;
			Options.m_ListenFlags = Internal.m_Settings.m_ListenFlags;
			Options.m_FriendlyName = Internal.m_Settings.f_GetCompositeFriendlyName() + "_CommandLine";
			Options.m_Enclave = NCryptography::fg_RandomID();
			Options.m_TranslateHostnames = Internal.m_TranslateHostnames;
			Options.m_InitialConnectionTimeout = 55.0;
			Options.m_DefaultConnectionConcurrency = -1;
			Options.m_bSupportAuthentication = false;

			Internal.m_TrustManager =
				fg_ConstructActor<CDistributedActorTrustManager>
				(
					fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>
					(
						fg_Format("{}/CommandLineTrustDatabase.{}", Internal.m_Settings.m_RootDirectory, Internal.m_Settings.m_AppName)
					)
					, fg_Move(Options)
				)
			;
			Internal.m_TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(60.0);
			Internal.m_DistributionManager = Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync();
			Internal.m_CommandLineSubscription = fg_ConstructActor<TCDistributedActorSingleSubscription<ICCommandLine>>
				(
					"com.malterlib/Concurrency/Commandline"
					, Internal.m_DistributionManager
				)
			;
			Internal.m_bInitialized = true;
		}
	}

	TCFuture<CDistributedAppCommandLineClient> CDistributedAppActor::f_GetCommandLineClient()
	{
		return fg_Explicit(CDistributedAppCommandLineClient(mp_Settings, mp_pCommandLineSpec, fp_GetTranslateHostnames()));
	}

	CDistributedAppCommandLineClient::CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other) = default;
	CDistributedAppCommandLineClient &CDistributedAppCommandLineClient::operator =(CDistributedAppCommandLineClient &&_Other) = default;
}
