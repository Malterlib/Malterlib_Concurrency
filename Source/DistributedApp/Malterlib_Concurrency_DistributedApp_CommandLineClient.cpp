// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Process/StdInActor>
#include <Mib/Process/Platform>
#include <Mib/CommandLine/CommandLineImplementation>

namespace NMib::NCommandLine
{
	template struct NCommandLine::TCCommandLineClient<NConcurrency::CCommandLineSpecificationDistributedAppCustomization, NConcurrency::CDistributedAppCommandLineClient>;
}

namespace NMib::NConcurrency
{
	struct CDistributedAppCommandLineClient::CInternal
	{
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCActor<TCDistributedActorSingleSubscription<ICCommandLine>> m_CommandLineSubscription;
		CDistributedAppActor_Settings m_Settings;
		NContainer::TCMap<NStr::CStr, NStr::CStr> m_TranslateHostnames;
		bool m_bInitialized = false;
	};

	void CDistributedAppCommandLineClient::f_SetLazyStartApp
		(
			NFunction::TCFunction<FStopApp (NEncoding::CEJSONSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyStartApp
		)
	{
		mp_fLazyStartApp = _fLazyStartApp;
	}

	void CDistributedAppCommandLineClient::f_SetLazyPreRunDirectCommand
		(
			NFunction::TCFunction<void (NEncoding::CEJSONSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyPreRunDirectCommand
		)
	{
		mp_fLazyPreRunDirectCommand = _fLazyPreRunDirectCommand;
	}

	namespace
	{
		struct CCommandLineControlActor : public ICCommandLineControl
		{
			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();

				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_RegisterForInputBinary, fg_Move(_fOnInput), _Flags, CActorDistributionManager::mc_HalfMaxMessageSize);
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForCancellation(FOnCancel &&_fOnCancel) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				auto SubscriptionID = NCryptography::fg_RandomID(mp_CancellationSubscriptions);

				auto &Subscription = mp_CancellationSubscriptions[SubscriptionID];

				Subscription.m_fOnCancel = fg_Move(_fOnCancel);

				if (mp_bCancelled)
					Subscription.m_fOnCancel() > fg_DiscardResult();

				co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
					{
						if (auto pSubscription = mp_CancellationSubscriptions.f_FindEqual(SubscriptionID))
							co_await fg_Move(pSubscription->m_fOnCancel).f_Destroy();

						mp_CancellationSubscriptions.f_Remove(SubscriptionID);

						co_return {};
					}
				;
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();

				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_RegisterForInput, fg_Move(_fOnInput), _Flags, CActorDistributionManager::mc_HalfMaxMessageSize);
			}

			TCFuture<NContainer::CSecureByteVector> f_ReadBinary() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_ReadBinary);
			}

			TCFuture<NStr::CStrSecure> f_ReadLine() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_ReadLine);
			}

			TCFuture<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_ReadPrompt, _Params);
			}

			TCFuture<void> f_AbortReads() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_AbortReads);
			}

			TCFuture<void> f_StdOut(NStr::CStrSecure const &_Output) override
			{
				NSys::fg_ConsoleOutput(_Output);
				co_return {};
			}

			TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) override
			{
				NSys::fg_ConsoleOutputBinary(_Output);
				co_return {};
			}

			TCFuture<void> f_StdErr(NStr::CStrSecure const &_Output) override
			{
				NSys::fg_ConsoleErrorOutput(_Output);
				co_return {};
			}

			TCFuture<bool> f_Cancel()
			{
				TCActorResultVector<bool> Results;

				mp_bCancelled = true;

				bool bDestroyApp = mp_CancellationSubscriptions.f_IsEmpty();

				for (auto &Subscription : mp_CancellationSubscriptions)
					Subscription.m_fOnCancel() > Results.f_AddResult();

				for (auto &bResult : co_await (co_await Results.f_GetResults() | g_Unwrap))
				{
					if (bResult)
						bDestroyApp = true;
				}

				co_return bDestroyApp;
			}

		private:

			struct CCancellationSubscription
			{
				FOnCancel m_fOnCancel;
			};

			TCActor<NProcess::CStdInActor> mp_InputActor;
			NContainer::TCMap<NStr::CStr, CCancellationSubscription> mp_CancellationSubscriptions;
			bool mp_bCancelled = false;

			TCFuture<void> fp_Destroy() override
			{
				if (mp_InputActor)
					co_await fg_Move(mp_InputActor).f_Destroy();

				for (auto &Subscription : mp_CancellationSubscriptions)
					co_await fg_Move(Subscription.m_fOnCancel).f_Destroy();

				co_return {};
			}

			void fp_CreateInputActor()
			{
				if (!mp_InputActor)
					mp_InputActor = fg_Construct();
			}
		};
	}

	uint32 CDistributedAppCommandLineClient::fp_RunCommand
		(
			void const *_pCommand
			, NEncoding::CEJSONSorted const &_Params
		)
	{
		CDistributedAppCommandLineSpecification::CInternal::CCommand const *pCommand = fg_AutoStaticCast(_pCommand);
		auto &Command = *pCommand;

		if (Command.m_pDirectRunCommand)
		{
			if (mp_fLazyPreRunDirectCommand)
				mp_fLazyPreRunDirectCommand(_Params, Command.m_Flags);

			return (*Command.m_pDirectRunCommand)(_Params, *this);
		}
		else if (Command.m_pActorRunCommand)
		{
			FStopApp fStopApp;
			if (mp_fLazyStartApp)
				fStopApp = mp_fLazyStartApp(_Params, Command.m_Flags);
			fp_Init();

			auto &Internal = *mp_pInternal;

			TCDistributedActor<CCommandLineControlActor> pCommandLineControl = Internal.m_DistributionManager->f_ConstructActor<CCommandLineControlActor>();

			auto CommandLineActor = Internal.m_CommandLineSubscription(&TCDistributedActorSingleSubscription<ICCommandLine>::f_GetActor).f_CallSync(30.0);

			CCommandLineControl CommandLineControl;
			CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

			CommandLineControl.m_CommandLineWidth = mp_CommandLineWidth;
			CommandLineControl.m_CommandLineHeight = mp_CommandLineHeight;
			CommandLineControl.m_AnsiFlags = mp_AnsiFlags;

			struct CState
			{
				NThread::CEventAutoReset m_Event;
				NThread::CMutual m_ResultLock;
				TCAsyncResult<uint32> m_Result;
				bool m_bAborted = false;
			};

			NStorage::TCSharedPointer<CState> pState = fg_Construct();

			CommandLineActor.f_CallActor(&ICCommandLine::f_RunCommandLine)
				(
					 Command.m_Names.f_GetFirst()
					 , _Params
					 , fg_Move(CommandLineControl)
				)
				> NPrivate::fg_DirectResultActor() / [pState](TCAsyncResult<uint32> &&_Result)
				{
					DMibLock(pState->m_ResultLock);
					pState->m_Result = fg_Move(_Result);
					pState->m_Event.f_Signal();
				}
			;

			auto Subscription = NProcess::NPlatform::fg_Process_WaitForTermination
				(
					[pState, pCommandLineControl]
					{
						pCommandLineControl(&CCommandLineControlActor::f_Cancel) > fg_DirectCallActor() / [pState](TCAsyncResult<bool> &&_Result)
							{
								if (!_Result)
									DMibConErrOut("Failed to cancel: {}\n", _Result.f_GetExceptionStr());

								if (*_Result)
								{
									DMibLock(pState->m_ResultLock);
									pState->m_bAborted = true;
									pState->m_Event.f_Signal();
								}
							}
						;
					}
				)
			;
			bool bStopped = false;

			TCAsyncResult<uint32> Result;
			while (true)
			{
				{
					DMibLock(pState->m_ResultLock);
					if (pState->m_Result.f_IsSet())
					{
						Result = pState->m_Result;
						break;
					}
					if (pState->m_bAborted)
					{
						DMibUnlock(pState->m_ResultLock);
						if (fStopApp)
						{
							bStopped = fStopApp();
							fStopApp.f_Clear();
						}

						if (!bStopped)
							break;
					}
				}
				pState->m_Event.f_Wait();
			}

			if (!Result.f_IsSet())
			{
				DMibLock(pState->m_ResultLock);
				if (!pState->m_Result.f_IsSet())
				{
					if (pState->m_bAborted)
						Result.f_SetException(DMibErrorInstance("Aborted"));
					else
						Result.f_SetException(DMibErrorInstance("No result"));
				}
				else
					 Result = pState->m_Result;
			}

			fg_Move(pCommandLineControl).f_Destroy().f_CallSync();

			return *Result;
		}
		return 0;
	}

	void CDistributedAppCommandLineClient::f_MutateCommandLineSpecification
		(
			NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
		)
	{
		auto &Internal = *mp_pInternal;
		_fMutate(*mp_pCommandLineSpecification, Internal.m_Settings);
	}

	CDistributedAppCommandLineClient::CDistributedAppCommandLineClient
		(
			CDistributedAppActor_Settings const &_Settings
			, NStorage::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
			, NContainer::TCMap<NStr::CStr, NStr::CStr> &&_TranslateHostnames
		)
		: NCommandLine::TCCommandLineClient<CCommandLineSpecificationDistributedAppCustomization, CDistributedAppCommandLineClient>(_pCommandLineSpecification)
		, mp_pInternal(fg_Construct())
	{
		auto &Internal = *mp_pInternal;
		Internal.m_Settings = _Settings;
		Internal.m_TranslateHostnames = fg_Move(_TranslateHostnames);
	}

	CDistributedAppCommandLineClient::~CDistributedAppCommandLineClient()
	{
		if (mp_pInternal && mp_pInternal->m_TrustManager)
			mp_pInternal->m_TrustManager->f_BlockDestroy();
	}

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
			Options.m_ReconnectDelay = Internal.m_Settings.m_ReconnectDelay;

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
		auto &Internal = *mp_pInternal;

		co_return CDistributedAppCommandLineClient(mp_Settings, Internal.m_pCommandLineSpec, fp_GetTranslateHostnames());
	}

	CDistributedAppCommandLineClient::CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other) = default;
	CDistributedAppCommandLineClient &CDistributedAppCommandLineClient::operator =(CDistributedAppCommandLineClient &&_Other) = default;
}
