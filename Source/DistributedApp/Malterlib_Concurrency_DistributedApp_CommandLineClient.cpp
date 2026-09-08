// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"
#include "../DistributedTrust/Malterlib_Concurrency_DistributedTrust_AuthActor_U2F.h"

#include <Mib/Concurrency/AsyncDestroy>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JsonDirectory>
#include <Mib/Process/StdInActor>
#include <Mib/Process/Platform>
#include <Mib/CommandLine/CommandLineImplementation>
#include <Mib/CommandLine/Platform>
#include <Mib/File/File>
#include <Mib/File/ChangeNotificationActor>

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
		NStorage::TCSharedPointer<CRunLoop> m_pRunLoop;
		TCWeakActor<CDistributedAppActor> m_AppActor; // Only set when the app lives in this process
		bool m_bInitialized = false;
	};

	void CDistributedAppCommandLineClient::f_SetLazyStartApp
		(
			NFunction::TCFunction<FStopApp (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyStartApp
		)
	{
		mp_fLazyStartApp = _fLazyStartApp;
	}

	void CDistributedAppCommandLineClient::f_SetLazyPreRunDirectCommand
		(
			NFunction::TCFunction<void (NEncoding::CEJsonSorted const &_Params, EDistributedAppCommandFlag _Flags)> const &_fLazyPreRunDirectCommand
		)
	{
		mp_fLazyPreRunDirectCommand = _fLazyPreRunDirectCommand;
	}

	NStorage::TCSharedPointer<CRunLoop> const &CDistributedAppCommandLineClient::f_GetRunLoop() const
	{
		auto &Internal = *mp_pInternal;
		return Internal.m_pRunLoop;
	}

	namespace
	{
		struct CCommandLineControlActor : public ICCommandLineControl
		{
			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput _fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();

				auto Subscription = co_await mp_InputActor(&NProcess::CStdInActor::f_RegisterForInputBinary, fg_Move(_fOnInput), _Flags, CActorDistributionManager::mc_HalfMaxMessageSize);

				co_return fp_TrackStdInRegistration(fg_Move(Subscription));
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForCancellation(FOnCancel _fOnCancel) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				auto SubscriptionID = NCryptography::fg_RandomID(mp_CancellationSubscriptions);

				auto &Subscription = mp_CancellationSubscriptions[SubscriptionID];

				Subscription.m_fOnCancel = fg_Move(_fOnCancel);

				if (mp_bCancelled)
					Subscription.m_fOnCancel.f_CallDiscard();

				co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
					{
						if (auto pSubscription = mp_CancellationSubscriptions.f_FindEqual(SubscriptionID))
							co_await fg_Move(pSubscription->m_fOnCancel).f_Destroy();

						mp_CancellationSubscriptions.f_Remove(SubscriptionID);

						co_return {};
					}
				;
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForScreenChange(FOnScreenChange _fOnScreenChange) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				// Screen changes reach this process through the terminal's input on Windows, where
				// the console input queue has one head and whoever reads it owns everything queued
				// ahead of a resize record. Requiring a standard input registration first keeps
				// that ownership with the reader the command already has, so no watcher ever has
				// to read, and drop, keystrokes typed ahead. The same rule holds on every platform
				// so a command behaves alike everywhere
				if (mp_StdInRegistrations.f_IsEmpty())
					co_return DMibErrorInstance("Screen change notifications require a registration for standard input first");

				auto SubscriptionID = NCryptography::fg_RandomID(mp_ScreenChangeSubscriptions);

				auto &Subscription = mp_ScreenChangeSubscriptions[SubscriptionID];

				Subscription.m_fOnScreenChange = fg_Move(_fOnScreenChange);

				fp_UpdateScreenChangeWatcher();

				co_return g_ActorSubscription / [this, SubscriptionID]() -> TCFuture<void>
					{
						if (auto pSubscription = mp_ScreenChangeSubscriptions.f_FindEqual(SubscriptionID))
							co_await fg_Move(pSubscription->m_fOnScreenChange).f_Destroy();

						mp_ScreenChangeSubscriptions.f_Remove(SubscriptionID);

						fp_UpdateScreenChangeWatcher();

						co_return {};
					}
				;
			}

			TCFuture<TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput _fOnInput, NProcess::EStdInReaderFlag _Flags) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();

				auto Subscription = co_await mp_InputActor(&NProcess::CStdInActor::f_RegisterForInput, fg_Move(_fOnInput), _Flags, CActorDistributionManager::mc_HalfMaxMessageSize);

				co_return fp_TrackStdInRegistration(fg_Move(Subscription));
			}

			TCFuture<NContainer::CIOByteVector> f_ReadBinary() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_ReadBinary);
			}

			TCFuture<NStr::CStrIO> f_ReadLine() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				fp_CreateInputActor();
				co_return co_await mp_InputActor(&NProcess::CStdInActor::f_ReadLine);
			}

			TCFuture<NStr::CStrIO> f_ReadPrompt(NProcess::CStdInReaderPromptParams _Params) override
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

			TCFuture<void> f_StdOut(NStr::CStrIO _Output) override
			{
				NSys::fg_ConsoleOutput(_Output);
				co_return {};
			}

			TCFuture<void> f_StdOutBinary(NContainer::CIOByteVector _Output) override
			{
				NSys::fg_ConsoleOutputBinary(_Output);
				co_return {};
			}

			TCFuture<void> f_StdErr(NStr::CStrIO _Output) override
			{
				NSys::fg_ConsoleErrorOutput(_Output);
				co_return {};
			}

			TCFuture<void> f_Clipboard_SetText(NStr::CStrIO _Text) override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				if (!NSys::fg_Clipboard_SetText(_Text))
					co_return DMibErrorInstance("Failed to store text in the system clipboard");

				co_return {};
			}

			TCFuture<NStr::CStrIO> f_Clipboard_GetText() override
			{
				if (auto Destroyed = fp_CheckDestroyed())
					co_return Destroyed;

				NStr::CStr Text;

				if (!NSys::fg_Clipboard_GetText(Text))
					co_return DMibErrorInstance("Failed to read text from the system clipboard");

				co_return NStr::CStrIO(fg_Move(Text));
			}

			TCFuture<bool> f_Cancel()
			{
				TCFutureVector<bool> Results;

				mp_bCancelled = true;

				bool bDestroyApp = mp_CancellationSubscriptions.f_IsEmpty();

				for (auto &Subscription : mp_CancellationSubscriptions)
					Subscription.m_fOnCancel() > Results;

				for (auto &bResult : co_await fg_AllDone(Results))
				{
					if (bResult)
						bDestroyApp = true;
				}

				co_return bDestroyApp;
			}

			NConcurrency::TCFuture<CU2FRegister::CResult> f_U2F_Register(CU2FRegister _Register) override
			{
				auto Result = co_await CU2FHelpers::fs_Register
					(
							CU2FHelpers::CU2FRegister
							{
								.m_ChallengeDigest = fg_Move(_Register.m_ChallengeDigest)
								, .m_AppDigest = fg_Move(_Register.m_AppDigest)
								, .m_Prompt = fg_Move(_Register.m_Prompt)
							}
					)
				;

				co_return
					{
						.m_PublicKey = fg_Move(Result.m_PublicKey)
						, .m_KeyHandle = fg_Move(Result.m_KeyHandle)
						, .m_AttestationCertificate = fg_Move(Result.m_AttestationCertificate)
						, .m_Signature = fg_Move(Result.m_Signature)
					}
				;
			}

			NConcurrency::TCFuture<CU2FAuthenticate::CResult> f_U2F_Authenticate(CU2FAuthenticate _Authenticate) override
			{
				CU2FHelpers::CU2FAuthenticate Authenticate{.m_Prompt = _Authenticate.m_Prompt};

				for (auto &Attempt : _Authenticate.m_Attempts)
				{
					Authenticate.m_Attempts.f_Insert
						(
							{
								.m_ChallengeDigest = fg_Move(Attempt.m_ChallengeDigest)
								, .m_AppDigest = fg_Move(Attempt.m_AppDigest)
								, .m_KeyHandle = fg_Move(Attempt.m_KeyHandle)
							}
						)
					;
				}

				auto Result = co_await CU2FHelpers::fs_Authenticate(fg_Move(Authenticate));

				co_return {.m_AppDigest = Result.m_AppDigest, .m_Signature = fg_Move(Result.m_Signature)};
			}

			TCFuture<void> f_ScreenChange(CScreenChange _ScreenChange)
			{
				TCFutureVector<void> Results;

				for (auto &Subscription : mp_ScreenChangeSubscriptions)
					Subscription.m_fOnScreenChange(_ScreenChange) > Results;

				co_await fg_AllDone(Results);

				co_return {};
			}

		private:
			struct CCancellationSubscription
			{
				FOnCancel m_fOnCancel;
			};

			struct CScreenChangeSubscription
			{
				FOnScreenChange m_fOnScreenChange;
			};

			TCActor<NProcess::CStdInActor> mp_InputActor;
			NContainer::TCMap<NStr::CStr, CCancellationSubscription> mp_CancellationSubscriptions;
			NContainer::TCMap<NStr::CStr, CScreenChangeSubscription> mp_ScreenChangeSubscriptions;

			// The command's live standard input registrations, by a key of this actor's own: the
			// wrapped subscription handed back tracks its removal here, which is what tells
			// whether screen change notifications may be offered and whether the watcher that
			// delivers them should be running
			NContainer::TCMap<NStr::CStr, TCActorSubscriptionWithID<>> mp_StdInRegistrations;

			// The platform's screen change delivery, held only while both a screen change
			// subscription and a standard input registration exist
			COnScopeExitShared mp_ScreenChangeWatcher;
			bool mp_bCancelled = false;

			TCFuture<void> fp_Destroy() override
			{
				// First, so no notification can arrive into an actor that is being torn down
				mp_ScreenChangeWatcher.f_Clear();

				for (auto &Registration : mp_StdInRegistrations)
				{
					if (Registration.f_GetSubscription())
						co_await Registration.f_GetSubscription()->f_Destroy();
				}

				if (mp_InputActor)
					co_await fg_Move(mp_InputActor).f_Destroy();

				for (auto &Subscription : mp_CancellationSubscriptions)
					co_await fg_Move(Subscription.m_fOnCancel).f_Destroy();

				for (auto &Subscription : mp_ScreenChangeSubscriptions)
					co_await fg_Move(Subscription.m_fOnScreenChange).f_Destroy();

				co_return {};
			}

			void fp_CreateInputActor()
			{
				if (!mp_InputActor)
					mp_InputActor = fg_Construct();
			}

			// Wraps a standard input registration so its lifetime is known here. The wrapper keeps
			// the input actor's own subscription and destroys it when the command drops the wrapper
			TCActorSubscriptionWithID<> fp_TrackStdInRegistration(TCActorSubscriptionWithID<> &&_Subscription)
			{
				auto RegistrationID = NCryptography::fg_RandomID(mp_StdInRegistrations);
				uint32 SubscriptionID = _Subscription.f_GetID();

				mp_StdInRegistrations[RegistrationID] = fg_Move(_Subscription);

				fp_UpdateScreenChangeWatcher();

				TCActorSubscriptionWithID<> Wrapped = g_ActorSubscription / [this, RegistrationID]() -> TCFuture<void>
					{
						auto *pRegistration = mp_StdInRegistrations.f_FindEqual(RegistrationID);
						if (!pRegistration)
							co_return {};

						auto Registration = fg_Move(*pRegistration);
						mp_StdInRegistrations.f_Remove(RegistrationID);

						// Without a reader of its own the command cannot be told about the screen
						// any more, see f_RegisterForScreenChange; its subscriptions stay and come
						// back to life with its next registration
						fp_UpdateScreenChangeWatcher();

						if (Registration.f_GetSubscription())
							co_await Registration.f_GetSubscription()->f_Destroy();

						co_return {};
					}
				;
				Wrapped.f_SetID(SubscriptionID);

				return Wrapped;
			}

			// Installs the platform's screen change delivery when it has a subscriber and standard
			// input is read, and drops it when either goes away. Installed from here rather than
			// for every command the client runs, so a command that never asks costs no watcher:
			// on Windows that is the console reader forwarding resize records, on POSIX a SIGWINCH
			// registration whose functor the signal subsystem dispatches on an io loop's thread.
			// The scope binds that loop to the pool, the same way the termination watcher is bound
			// when the command starts, so the subsystem never needs a thread of its own
			void fp_UpdateScreenChangeWatcher()
			{
				bool bWanted = !mp_ScreenChangeSubscriptions.f_IsEmpty() && !mp_StdInRegistrations.f_IsEmpty();
				if (bWanted == bool(mp_ScreenChangeWatcher))
					return;

				if (!bWanted)
				{
					mp_ScreenChangeWatcher.f_Clear();
					return;
				}

				CIoLoopCreateScope WatcherLoopScope(fg_ConcurrencyManager().f_PickIoLoopBinding(EPriority_Normal));

				mp_ScreenChangeWatcher = NCommandLine::NPlatform::fg_Process_WaitForScreenChange
					(
						[pThisWeak = fg_ThisActorWeak(this)](NSys::CConsoleProperties const &_ConsoleProperties)
						{
							auto pThis = pThisWeak.f_Lock();
							if (!pThis)
								return;

							CScreenChange ScreenChange
								{
									.m_Width = _ConsoleProperties.m_Width
									, .m_Height = _ConsoleProperties.m_Height
									, .m_GlyphWidth = _ConsoleProperties.m_GlyphWidth
									, .m_GlyphHeight = _ConsoleProperties.m_GlyphHeight
								}
							;

							pThis(&CCommandLineControlActor::f_ScreenChange, ScreenChange) > fg_DirectCallActor() / [](TCAsyncResult<void> &&_Result)
								{
									if (!_Result)
										DMibConErrOut("Failed to notify screen change: {}\n", _Result.f_GetExceptionStr());
								}
							;
						}
					)
				;
			}
		};
	}

	namespace
	{
		// Waits for the daemon to write its command line connection ticket into the command line
		// trust database before this client reads that database. On a cold first install the daemon
		// starts and writes the ticket (CommandLineTrustDatabase/ClientConnections/*.json)
		// asynchronously, while a client invocation (e.g. --trust-generate-ticket) may start and read
		// the trust database before the ticket exists. The client reads the trust database only once
		// (when constructing its trust manager), so without this wait it would build an empty trust
		// manager, never connect to the daemon, and time out waiting for the command line actor.

		TCFuture<void> fg_WaitForConnection(NStr::CStr _ClientConnectionsGlob, NStr::CStr _WatchPath, fp64 _Timeout)
		{
			auto Capture = co_await g_CaptureExceptions;

			auto fHasConnection = [_ClientConnectionsGlob]() -> bool
				{
					return !NMib::NFile::CFile::fs_FindFiles(_ClientConnectionsGlob).f_IsEmpty();
				}
			;

			// Fast path: the ticket is already present (warm daemon / subsequent runs).
			if (fHasConnection())
				co_return {};

			TCActor<NMib::NFile::CFileChangeNotificationActor> WatchActor = fg_ConstructActor<NMib::NFile::CFileChangeNotificationActor>();

			auto CleanupWatchActor = co_await fg_AsyncDestroy(WatchActor);

			TCPromiseFuturePair<void> Pair;

			// Watch the (always-existing) root directory recursively (EFileChange_All includes
			// EFileChange_Recursive) so we are notified when the daemon creates the ticket below it.
			auto RegisterResult = co_await WatchActor
				(
					&NMib::NFile::CFileChangeNotificationActor::f_RegisterForChanges
					, _WatchPath
					, NMib::NFile::EFileChange_All
					, g_ActorFunctor / [fHasConnection, Promise = Pair.m_Promise, bSet = false]
					(NContainer::TCVector<NMib::NFile::CFileChangeNotification::CNotification>) mutable -> TCFuture<void>
					{
						auto Capture = co_await g_CaptureExceptions;

						if (!bSet && fHasConnection())
						{
							bSet = true;
							Promise.f_SetResult();
						}

						co_return {};
					}
					, NMib::NFile::CFileChangeNotificationActor::CCoalesceSettings{.m_Delay = 0.0}
				).f_Wrap()
			;

			if (!RegisterResult)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Warning, "Failed to watch for command line connection ticket; proceeding without waiting: {}", RegisterResult.f_GetExceptionStr());
				co_return {};
			}

			CActorSubscription Subscription = fg_Move(*RegisterResult);

			// Re-check after registering: the ticket may have appeared between the fast-path check
			// and the watch being established.
			if (!fHasConnection())
			{
				auto WaitResult = co_await fg_Move(Pair.m_Future).f_Timeout(_Timeout, "Timed out waiting for command line connection ticket").f_Wrap();
				if (!WaitResult)
					DMibLogWithCategory(Mib/Concurrency/App, Warning, "Proceeding without command line connection ticket: {}", WaitResult.f_GetExceptionStr());
			}

			if (Subscription)
				co_await fg_Exchange(Subscription, nullptr)->f_Destroy();

			co_return {};
		}
	}

	uint32 CDistributedAppCommandLineClient::fp_RunCommand
		(
			void const *_pCommand
			, NEncoding::CEJsonSorted &&_Params
		)
	{
		CDistributedAppCommandLineSpecification::CInternal::CCommand const *pCommand = fg_AutoStaticCast(_pCommand);
		auto &Command = *pCommand;

		if (Command.m_pDirectRunCommand)
		{
			if (mp_fLazyPreRunDirectCommand)
				mp_fLazyPreRunDirectCommand(_Params, Command.m_Flags);

			return (*Command.m_pDirectRunCommand)(fg_Move(_Params), *this);
		}
		else if (Command.m_pActorRunCommand)
		{
			FStopApp fStopApp;
			if (mp_fLazyStartApp)
				fStopApp = mp_fLazyStartApp(_Params, Command.m_Flags);

			auto &Internal = *mp_pInternal;

			TCActor<CActorDistributionManager> DistributionManager;
			TCDistributedActor<ICCommandLine> CommandLineActor;

			// An app that runs in this process hands us its command line actor directly. Calls on a
			// distributed actor that was constructed locally are ordinary actor calls, so this skips
			// the client side trust manager and distribution manager, the connection to the app's
			// local socket, and the wait for the command line publication to arrive over it.
			// f_GetInProcessCommandLine returns nothing unless the app opted in with
			// f_InProcessCommandLineOnly and actually started in this process, so every other case
			// falls through to the connecting path below
			if (auto AppActor = Internal.m_AppActor.f_Lock())
			{
				auto InProcess = AppActor(&CDistributedAppActor::f_GetInProcessCommandLine).f_CallSync(Internal.m_pRunLoop);

				CommandLineActor = fg_Move(InProcess.m_CommandLine);
				DistributionManager = fg_Move(InProcess.m_DistributionManager);
			}

			bool bRemoteApp = !CommandLineActor;

			if (!CommandLineActor)
			{
				fp_Init(_Params);

				DistributionManager = Internal.m_DistributionManager;

				CommandLineActor = Internal.m_CommandLineSubscription(&TCDistributedActorSingleSubscription<ICCommandLine>::f_GetActor)
					.f_Timeout(30.0, "Timed out waiting for command line actor to appear")
					.f_CallSync(Internal.m_pRunLoop)
				;
			}

			TCDistributedActor<CCommandLineControlActor> pCommandLineControl = DistributionManager->f_ConstructActor<CCommandLineControlActor>();

			CCommandLineControl CommandLineControl;
			CommandLineControl.m_ControlActor = pCommandLineControl->f_ShareInterface<ICCommandLineControl>();

			CommandLineControl.m_CommandLineWidth = mp_CommandLineWidth;
			CommandLineControl.m_CommandLineHeight = mp_CommandLineHeight;
			CommandLineControl.m_CommandLineGlyphWidth = mp_CommandLineGlyphWidth;
			CommandLineControl.m_CommandLineGlyphHeight = mp_CommandLineGlyphHeight;
			CommandLineControl.m_AnsiFlags = mp_AnsiFlags;
			CommandLineControl.m_ClientInfo = CCommandLineClientInfo::fs_CollectLocal(bRemoteApp);

			struct CState
			{
				NThread::CMutual m_ResultLock;
				TCAsyncResult<uint32> m_Result;
				bool m_bAborted = false;

				NStorage::TCSharedPointer<CRunLoop> m_pRunLoop;
			};

			NStorage::TCSharedPointer<CState> pState = fg_Construct();
			pState->m_pRunLoop = Internal.m_pRunLoop;

			CommandLineActor.f_CallActor(&ICCommandLine::f_RunCommandLine)
				(
					 Command.m_Names.f_GetFirst()
					 , fg_Move(_Params)
					 , fg_Move(CommandLineControl)
				)
				.f_OnResultSet
				(
					[pState](TCAsyncResult<uint32> &&_Result)
					{
						DMibLock(pState->m_ResultLock);
						pState->m_Result = fg_Move(_Result);
						pState->m_pRunLoop->f_Wake();
					}
				)
			;

			// The watcher turns a signal into a callback, and the signal subsystem dispatches it on
			// an io loop's thread when the first registration happens inside this scope. Without
			// it the subsystem keeps a thread of its own, which for a command line that runs once
			// and exits is a thread started and joined for a signal that will not arrive.
			// The scope closes as soon as the registration is made: it is a thread local, and
			// leaving it open would bind every io object the command itself creates to this loop.
			// Screen changes are not watched here: the control actor installs that watcher when
			// a command subscribes, so a command that never asks pays nothing for it
			COnScopeExitShared TerminationSubscription;

			{
				CIoLoopCreateScope SignalLoopScope(fg_ConcurrencyManager().f_PickIoLoopBinding(EPriority_Normal));

				TerminationSubscription = NProcess::NPlatform::fg_Process_WaitForTermination
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
										pState->m_pRunLoop->f_Wake();
									}
								}
							;
						}
					)
				;
			}

			bool bStopped = false;

			TCAsyncResult<uint32> Result;
			while (true)
			{
				bool bDoneSomething = false;
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
							bDoneSomething = true;
						}

						if (!bStopped)
							break;
					}
				}
				if (!bDoneSomething)
					pState->m_pRunLoop->f_WaitOnce();
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

			fg_Move(pCommandLineControl).f_Destroy().f_CallSync(Internal.m_pRunLoop);

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
			, NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
			, TCWeakActor<CDistributedAppActor> const &_AppActor
		)
		: NCommandLine::TCCommandLineClient<CCommandLineSpecificationDistributedAppCustomization, CDistributedAppCommandLineClient>(_pCommandLineSpecification)
		, mp_pInternal(fg_Construct())
	{
		auto &Internal = *mp_pInternal;
		Internal.m_Settings = _Settings;
		Internal.m_TranslateHostnames = fg_Move(_TranslateHostnames);
		Internal.m_pRunLoop = _pRunLoop;
		Internal.m_AppActor = _AppActor;
	}

	CDistributedAppCommandLineClient::~CDistributedAppCommandLineClient()
	{
		if (mp_pInternal)
		{
			auto &Internal = *mp_pInternal;
			if (Internal.m_CommandLineSubscription)
				Internal.m_CommandLineSubscription->f_BlockDestroy(Internal.m_pRunLoop->f_ActorDestroyLoop());
			if (Internal.m_TrustManager)
				Internal.m_TrustManager->f_BlockDestroy(Internal.m_pRunLoop->f_ActorDestroyLoop());
		}
	}

	void CDistributedAppCommandLineClient::fp_Init(NEncoding::CEJsonSorted const &_Params)
	{
		using namespace NStr;

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

			CStr TrustDatabase;
			CStr RemoteCommandLineHostID;
			if (auto pValue = _Params.f_GetMember("RemoteCommandLine"); pValue && pValue->f_Boolean())
			{
				TrustDatabase = Internal.m_Settings.m_RootDirectory  / ("TrustDatabase.{}"_f << Internal.m_Settings.m_AppName);

				if (auto pValue = _Params.f_GetMember("RemoteCommandLineHost"))
					RemoteCommandLineHostID = pValue->f_String();
			}
			else
			{
				TrustDatabase = Internal.m_Settings.m_RootDirectory / ("CommandLineTrustDatabase.{}"_f << Internal.m_Settings.m_AppName);

				// The trust database below is read only once (when the trust manager is constructed),
				// so on a cold first install we must not read it before the daemon has written its
				// command line connection ticket. Wait (via file change notifications) for the ticket
				// to appear; best-effort, so a missing daemon still falls through after the timeout.
				auto BlockingActor = fg_BlockingActor();
				(
					g_Dispatch(BlockingActor) / [TrustDatabase, RootDir = Internal.m_Settings.m_RootDirectory] -> TCFuture<void>
					{
						CStr ClientConnectionsGlob = TrustDatabase / "ClientConnections/*.json";

						co_await fg_WaitForConnection(ClientConnectionsGlob, RootDir, 10.0);

						co_return {};
					}
				).f_CallSync(Internal.m_pRunLoop, 60.0);
			}

			Internal.m_TrustManager = fg_ConstructActor<CDistributedActorTrustManager>
				(
					fg_ConstructActor<CDistributedActorTrustManagerDatabase_JsonDirectory>(TrustDatabase)
					, fg_Move(Options)
				)
			;

			Internal.m_TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(Internal.m_pRunLoop, 60.0);
			Internal.m_DistributionManager = Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync(Internal.m_pRunLoop);
			Internal.m_CommandLineSubscription = fg_ConstructActor<TCDistributedActorSingleSubscription<ICCommandLine>>
				(
					TCDistributedActorSingleSubscription<ICCommandLine>::CFilter
					{
						.m_Namespace = "com.malterlib/Concurrency/Commandline"
						, .m_HostID = RemoteCommandLineHostID
					}
					, Internal.m_DistributionManager
				)
			;
			Internal.m_bInitialized = true;
		}
	}

	TCFuture<CDistributedAppCommandLineClient> CDistributedAppActor::f_GetCommandLineClient(NStorage::TCSharedPointer<CRunLoop> _pRunLoop)
	{
		auto &Internal = *mp_pInternal;

		co_return CDistributedAppCommandLineClient(mp_Settings, Internal.m_pCommandLineSpec, fp_GetTranslateHostnames(), _pRunLoop, fg_ThisActor(this));
	}

	CDistributedAppCommandLineClient::CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other) = default;
	CDistributedAppCommandLineClient &CDistributedAppCommandLineClient::operator =(CDistributedAppCommandLineClient &&_Other) = default;
}
