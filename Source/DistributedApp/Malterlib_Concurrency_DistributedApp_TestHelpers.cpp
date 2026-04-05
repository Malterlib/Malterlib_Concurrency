// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedApp_TestHelpers.h"

#include <Mib/File/ChangeNotificationActor>
#include <Mib/CommandLine/CommandLine>

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NContainer;

	struct CDistributedAppLogForwarder::CInternal : public CActorInternal
	{
		struct CLogState
		{
			uint64 m_LastLength = 0;
			CFile m_File;
			CStr m_LastPath;
		};

		CInternal(CStr const &_RootPath)
			: m_RootPath(_RootPath)
		{
		}

		TCFuture<void> f_UpdateFile(CUniqueFileIdentifier _Identifier);

		CStr m_RootPath;

		TCActor<CFileChangeNotificationActor> m_FileChangeNotificationActor;
		CActorSubscription m_FileChangeNotificationSubscription;

		TCMap<CUniqueFileIdentifier, CLogState> m_LogStates;
		TCMap<CStr, CLogState *> m_FileToLogState;

		CActorSubscription m_UpdateAllTimerSubscription;
	};

	CDistributedAppLogForwarder::CDistributedAppLogForwarder(CStr const &_RootPath)
		: mp_pInternal(fg_Construct(_RootPath))
	{
	}

	CDistributedAppLogForwarder::~CDistributedAppLogForwarder() = default;

	TCFuture<void> CDistributedAppLogForwarder::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		if (Internal.m_UpdateAllTimerSubscription)
			co_await fg_Exchange(Internal.m_UpdateAllTimerSubscription, nullptr)->f_Destroy();

		if (Internal.m_FileChangeNotificationSubscription)
			co_await fg_Exchange(Internal.m_FileChangeNotificationSubscription, nullptr)->f_Destroy();

		co_await fg_Move(Internal.m_FileChangeNotificationActor).f_Destroy();

		co_return {};
	}

	TCFuture<void> CDistributedAppLogForwarder::CInternal::f_UpdateFile(CUniqueFileIdentifier _Identifier)
	{
		auto &State = m_LogStates[_Identifier];

		try
		{
			uint64 Length = State.m_File.f_GetLength();
			DMibFastCheck(State.m_File.f_IsValid());

			if (Length < State.m_LastLength)
				State.m_LastLength = 0;

			State.m_File.f_SetPosition(State.m_LastLength);

			auto ToRead = Length - State.m_LastLength;

			State.m_LastLength = Length;

			CStr Contents;
			auto pOutBuffer = Contents.f_GetStr(ToRead + 1);
			State.m_File.f_Read(pOutBuffer, ToRead);
			pOutBuffer[ToRead] = 0;
			Contents.f_SetStrLen(ToRead);

			Contents = Contents.f_Trim();

			if (!Contents.f_IsEmpty())
			{
				DMibLogOperation(Log);
				for ([[maybe_unused]] auto &Line : Contents.f_SplitLine<false>())
					DMibLogWithCategoryStr(State.m_LastPath, Info, "{}", Line);
			}
		}
		catch ([[maybe_unused]] NException::CException const &_Exception)
		{
			DMibLog(Error, "Error monitoring for log forwarding '{}': {}", State.m_LastPath, _Exception);
		}

		co_return {};
	}

	TCFuture<void> CDistributedAppLogForwarder::f_StartMonitoring()
	{
		auto &Internal = *mp_pInternal;

		if (NCommandLine::CCommandLineDefaults::fs_ColorAnsiFlagsDefault() & NCommandLine::EAnsiEncodingFlag_Color)
			fg_GetSys()->f_SetEnvironmentVariable("MalterlibFileLogColor", "true");

		fg_GetSys()->f_SetEnvironmentVariable("MalterlibLogCategoryWidth", "");

		Internal.m_FileChangeNotificationActor = fg_Construct();

		if (!CFile::fs_FileExists(Internal.m_RootPath, EFileAttrib_Directory))
			co_return DMibErrorInstance("Monitored path '{}' doesn't exist"_f << Internal.m_RootPath);

		Internal.m_FileChangeNotificationSubscription = co_await Internal.m_FileChangeNotificationActor
			(
				&CFileChangeNotificationActor::f_RegisterForChanges
				, Internal.m_RootPath
				, EFileChange_All
				, g_ActorFunctor / [this](TCVector<CFileChangeNotification::CNotification> _Changes) -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;

					for (auto &Change : _Changes)
					{
						if (!Change.m_Path.f_EndsWith(".log"))
							continue;

						auto FullPath = CFile::fs_GetExpandedPath(Change.m_Path, Internal.m_RootPath);

						try
						{
							if (Change.m_Notification == EFileChangeNotification_Removed)
							{
								continue;
							}

							CFile File;
							CUniqueFileIdentifier Identifier;
							try
							{
								File.f_Open(FullPath, EFileOpen_Read | EFileOpen_ReadAttribs | EFileOpen_ShareAll | EFileOpen_ShareBypass | EFileOpen_NoLocalCache);
								Identifier = File.f_GetUniqueIdentifier();
							}
							catch (NException::CException const &)
							{
								auto *pState = Internal.m_FileToLogState.f_FindEqual(FullPath);
								if (!pState)
									throw;

								Identifier = Internal.m_LogStates.fs_GetKey(*pState);
							}

							auto &State = Internal.m_LogStates[Identifier];
							State.m_LastPath = Change.m_Path;
							Internal.m_FileToLogState[FullPath] = &State;

							if (!State.m_File.f_IsValid())
								State.m_File = fg_Move(File);

							co_await Internal.f_UpdateFile(Identifier);
						}
						catch ([[maybe_unused]] NException::CException const &_Exception)
						{
							DMibLog(Error, "Error monitoring for log forwarding '{}': {}", Change.m_Path, _Exception);
						}
					}
					co_return {};
				}
				, CFileChangeNotificationActor::CCoalesceSettings{.m_Delay = 0.0}
			)
		;

		Internal.m_UpdateAllTimerSubscription = co_await fg_RegisterTimer
			(
				0.1
				, [this]() -> TCFuture<void>
				{
					auto &Internal = *mp_pInternal;

					TCVector<CUniqueFileIdentifier> ToUpdate;

					for (auto &Identifier : Internal.m_LogStates.f_Keys())
						ToUpdate.f_Insert(Identifier);

					for (auto &Identifier : ToUpdate)
						co_await Internal.f_UpdateFile(Identifier);

					co_return {};
				}
			)
		;

		co_return {};
	}
}

