// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/CommandLine/CommandLineImplementation>
#include <Mib/Concurrency/AsyncDestroy>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NProcess;
	using namespace NCommandLine;
	using namespace NException;

	template <typename t_CCommandLineSpecification>
 	typename t_CCommandLineSpecification::CCommand CCommandLineSpecificationDistributedAppCustomization::TCSection<t_CCommandLineSpecification>::CSection::f_RegisterCommand
		(
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJsonSorted &&_Params, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
	{
		typename t_CCommandLineSpecification::CInternal::CSection *pSection = fg_AutoStaticCast(this->mp_pSection);
		auto &Section = *pSection;
		auto &Internal = *(this->mp_pInternal);
		auto *pCommand = Internal.f_RegisterCommand(Section, fg_Move(_CommandDescription));
		pCommand->m_pActorRunCommand = fg_Construct(g_ActorFunctor / fg_Move(_fRunCommand));
		pCommand->m_Flags = _Flags;
		return typename t_CCommandLineSpecification::CCommand(this->mp_pInternal, pCommand);
	}

	template <typename t_CCommandLineSpecification>
	typename t_CCommandLineSpecification::CCommand CCommandLineSpecificationDistributedAppCustomization::TCSection<t_CCommandLineSpecification>::CSection::f_RegisterDirectCommand
		(
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJsonSorted &&_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
	{
		typename t_CCommandLineSpecification::CInternal::CSection *pSection = fg_AutoStaticCast(this->mp_pSection);
		auto &Section = *pSection;
		auto &Internal = *(this->mp_pInternal);
		auto *pCommand = Internal.f_RegisterCommand(Section, fg_Move(_CommandDescription));
		pCommand->m_pDirectRunCommand = fg_Construct(fg_Move(_fRunCommand));
		pCommand->m_Flags = _Flags;
		return typename t_CCommandLineSpecification::CCommand(this->mp_pInternal, pCommand);
	}

	template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterCommand
		(
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJsonSorted &&_Params, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;

	template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterDirectCommand
		(
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJsonSorted &&_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;
}

namespace NMib::NCommandLine
{
	template struct TCCommandLineSpecification<NConcurrency::CCommandLineSpecificationDistributedAppCustomization>;
}

namespace NMib::NConcurrency
{
	TCFuture<void> CDistributedAppActor::fp_PreRunCommandLine
		(
			 CStr _Command
			 , CEJsonSorted const _Params
			 , TCSharedPointer<CCommandLineControl> _pCommandLine
		)
	{
		co_return {};
	}

	TCFuture<uint32> CDistributedAppActor::f_RunCommandLine
		(
			CStr _Command
			, CEJsonSorted const _Params
			, TCSharedPointer<CCommandLineControl> _pCommandLine
		)
	{
		auto &Internal = *mp_pInternal;

		auto Auditor = f_Auditor();
		auto &CallingHostInfo = Auditor.f_HostInfo();

		if (!fp_HasCommandLineAccess(CallingHostInfo.f_GetRealHostID()))
		{
			TCVector<CStr> Permissions = {"DistributedApp/RunCommandLine"};

			bool bHasPermission = co_await (Internal.m_Permissions.f_HasPermission("Run command line", Permissions) % "Permission denied running command line" % Auditor);
			if (!bHasPermission)
				co_return Auditor.f_AccessDenied("(Run command line)", Permissions);
		}

		auto &SpecInternal = Internal.m_pCommandLineSpec->f_AccessInternal();
		auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);

		if (!pCommand)
			co_return DMibErrorInstance("Non-existant command line command");

		CEJsonSorted ValidatedParams;
		{
			auto CaptureScope = co_await g_CaptureExceptions;
			ValidatedParams = SpecInternal.f_ValidateParams(**pCommand, _Params);
		}

		if ((*pCommand)->m_Flags & EDistributedAppCommandFlag_WaitForRemotes)
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_WaitForInitialConnection);

		int64 AuthenticationLifetime = ValidatedParams.f_GetMemberValue("AuthenticationLifetime", CPermissionRequirements::mc_OverrideLifetimeNotSet).f_Integer();
		if (AuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet && _Command == "--trust-user-authenticate-pattern")
			AuthenticationLifetime = CPermissionRequirements::mc_DefaultMaximumLifetime;

		CStr UserID = ValidatedParams.f_GetMemberValue("AuthenticationUser", "").f_String();

		auto AuthenticationSetupResult = co_await fp_SetupAuthentication(_pCommandLine, AuthenticationLifetime, UserID);

		auto Cleanup = co_await fg_AsyncDestroy(fg_Move(AuthenticationSetupResult.m_Subscription));

		if (AuthenticationSetupResult.m_HandlerID)
		{
			CAuthenticationHandlerIDScope HandlerScope(AuthenticationSetupResult.m_HandlerID);
			co_await fp_PreRunCommandLine(_Command, ValidatedParams, _pCommandLine);
		}
		else
			co_await fp_PreRunCommandLine(_Command, ValidatedParams, _pCommandLine);

		pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
		if (!pCommand)
			co_return DMibErrorInstance("Non-existant command line command");

		auto &Command = **pCommand;
		if (!Command.m_pActorRunCommand)
			co_return DMibErrorInstance("Non-actor cammand");

		{
			auto CaptureScope = co_await g_CaptureExceptions;

			if (AuthenticationSetupResult.m_HandlerID)
			{
				CAuthenticationHandlerIDScope HandlerScope(AuthenticationSetupResult.m_HandlerID);
				uint32 Result = co_await (*Command.m_pActorRunCommand)(ValidatedParams, _pCommandLine);
				co_await _pCommandLine->f_StdOut(""); // Syncronize with output
				co_return Result;
			}
			else
			{
				uint32 Result = co_await (*Command.m_pActorRunCommand)(ValidatedParams, _pCommandLine);
				co_await _pCommandLine->f_StdOut(""); // Syncronize with output
				co_return Result;
			}
		}
	}

	TCFuture<uint32> CDistributedAppActor::fp_RunCommandLineAndLogError
		(
			CStr _Description
			, NFunction::TCFunctionMovable<TCFuture<uint32> ()> _fCommand
		)
	{
		auto Result = co_await fg_Dispatch(fg_Move(_fCommand)).f_Wrap();

		if (!Result)
			DMibLogWithCategory(Mib/Concurrency/App, Error, "{} failed from command line: {}", _Description, Result.f_GetExceptionStr());

		co_return fg_Move(Result);
	}

	void CDistributedAppActor::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
	}
}
