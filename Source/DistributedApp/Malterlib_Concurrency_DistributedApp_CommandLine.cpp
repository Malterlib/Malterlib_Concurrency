// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NProcess;
	using namespace NCommandLine;
	using namespace NException;

	TCFuture<void> CDistributedAppActor::fp_PreRunCommandLine
		(
			 CStr const &_Command
			 , CEJSON const &_Params
			 , TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_return {};
	}

	TCFuture<uint32> CDistributedAppActor::f_RunCommandLine
		(
			CCallingHostInfo const &_CallingHost
			, CStr const &_Command
			, CEJSON const &_Params
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		if (!fp_HasCommandLineAccess(_CallingHost.f_GetRealHostID()))
			co_return DMibErrorInstance("Access denied");

		auto &SpecInternal = *(mp_pCommandLineSpec->mp_pInternal);
		auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);

		if (!pCommand)
			co_return DMibErrorInstance("Non-existant command line command");

		CEJSON ValidatedParams;
		try
		{
			ValidatedParams = SpecInternal.f_ValidateParams(**pCommand, _Params);
		}
		catch (CException const &_Exception)
		{
			co_return _Exception.f_ExceptionPointer();
		}

		if ((*pCommand)->m_Flags & CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes)
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_WaitForInitialConnection);

		CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};

		int64 AuthenticationLifetime = ValidatedParams.f_GetMemberValue("AuthenticationLifetime", CPermissionRequirements::mc_OverrideLifetimeNotSet).f_Integer();
		if (AuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet && _Command == "--trust-user-authenticate-pattern")
			AuthenticationLifetime = CPermissionRequirements::mc_DefaultMaximumLifetime;

		CStr UserID = ValidatedParams.f_GetMemberValue("AuthenticationUser", "").f_String();

		auto AuthenticationSubscription = co_await self(&CDistributedAppActor::fp_SetupAuthentication, _pCommandLine, AuthenticationLifetime, UserID);

		co_await self(&CDistributedAppActor::fp_PreRunCommandLine, _Command, ValidatedParams, _pCommandLine);

		pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
		if (!pCommand)
			co_return DMibErrorInstance("Non-existant command line command");

		auto &Command = **pCommand;
		if (!Command.m_pActorRunCommand)
			co_return DMibErrorInstance("Non-actor cammand");

		try
		{
			uint32 Result = co_await (*Command.m_pActorRunCommand)(ValidatedParams, _pCommandLine);
			co_return Result;
		}
		catch (CException const &_Exception)
		{
			co_return _Exception.f_ExceptionPointer();
		}
	}

	TCFuture<uint32> CDistributedAppActor::fp_RunCommandLineAndLogError
		(
			CStr const &_Description
			, NFunction::TCFunctionMovable<TCFuture<uint32> ()> &&_fCommand
		)
	{
		TCPromise<uint32> Promise;
		fg_Dispatch
			(
				fg_Move(_fCommand)
			)
			> [Promise, _Description](TCAsyncResult<uint32> &&_Other)
			{
				if (!_Other)
					DMibLogWithCategory(Mib/Concurrency/App, Error, "{} failed from command line: {}", _Description, _Other.f_GetExceptionStr());
				Promise.f_SetResult(fg_Move(_Other));
			}
		;

		return Promise.f_MoveFuture();
	}

	void CDistributedAppActor::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
	}

	EAnsiEncodingFlag CDistributedAppActor::fs_ColorAnsiFlagsDefault()
	{
		EAnsiEncodingFlag Flags = EAnsiEncodingFlag_None;
		if (fs_ColorEnabledDefault())
			Flags |= EAnsiEncodingFlag_Color;
		if (fs_Color24BitEnabledDefault())
			Flags |= EAnsiEncodingFlag_Color24Bit;
		if (fs_ColorLightBackgroundDefault())
			Flags |= EAnsiEncodingFlag_ColorLightBackground;
		return Flags;
	}

	bool CDistributedAppActor::fs_ColorEnabledDefault()
	{
		static bool bValue = []
			{
				if (auto Value = fg_GetSys()->f_GetEnvironmentVariable("MalterlibColor", ""))
					return Value == "true";

				if (NSys::fg_System_BeingDebugged())
					return false;

				auto ColorTerm = fg_GetSys()->f_GetEnvironmentVariable("COLORTERM", "");
				if (ColorTerm == "truecolor" || ColorTerm == "24bit")
					return true;

#ifdef DPlatformFamily_Windows
				if (CSystem::ms_PlatformVersion >= 10'0'015063)
					return true;
				else
					return false;
#endif
				return true;
			}
			()
		;
		return bValue;
	}

	bool CDistributedAppActor::fs_Color24BitEnabledDefault()
	{
		static bool bValue = []
			{
				if (auto Value = fg_GetSys()->f_GetEnvironmentVariable("MalterlibColor24Bit", ""))
					return Value == "true";

				auto ColorTerm = fg_GetSys()->f_GetEnvironmentVariable("COLORTERM", "");
				if (ColorTerm == "truecolor" || ColorTerm == "24bit")
					return true;

#ifdef DPlatformFamily_Windows
				if (CSystem::ms_PlatformVersion >= 10'0'015063)
					return true;
#endif
				return false;
			}
			()
		;
		return bValue;
	}

	bool CDistributedAppActor::fs_ColorLightBackgroundDefault()
	{
		static bool bValue = []
			{
				if (auto Value = fg_GetSys()->f_GetEnvironmentVariable("MalterlibColorLight", ""); Value == "true")
					return true;
				else if (Value != "auto")
					return false;

				struct CState
				{
					NThread::CEvent m_Event;
					CStr m_Buffer;
					bool m_bLight = false;
				};

				TCSharedPointer<CState> pState = fg_Construct();

				auto Params = CStdInReaderParams::fs_Create
					(
						[pState](EStdInReaderOutputType _Type, CStrSecure const &_Input)
						{
							if (_Type != EStdInReaderOutputType_StdIn)
								return;
							CByteVector Data((uint8 const *)_Input.f_GetStr(), _Input.f_GetLen());

							pState->m_Buffer += _Input;
							if (auto iFound = pState->m_Buffer.f_Find("\x1B]11;rgb:"); iFound >= 0)
							{
								CStr Buffer = pState->m_Buffer.f_Extract(iFound);

								uint8 Red;
								uint8 Green;
								uint8 Blue;
								aint nParsed;
								(CStr::CParse("\x1B]11;rgb:{nfh}/{nfh}/{nfh}\x07") >> Red >> Green >> Blue).f_Parse(Buffer, nParsed);
								if (nParsed == 3)
								{
									pState->m_bLight = (fp32(Red) * 0.3f + fp32(Green) * 0.59f + fp64(Blue) * 0.11f) > 128.0f;
									pState->m_Event.f_SetSignaled();
								}
							}
						}
					)
				;

				CStdInReader StdIn(fg_Move(Params));

				DMibConOut2("\x1B]11;?\x1B\\");

				if (pState->m_Event.f_WaitTimeout(1.0))
					return false;

				return pState->m_bLight;
			}
			()
		;
		return bValue;
	}

}
