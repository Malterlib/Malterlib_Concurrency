// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/CommandLineImplementation>

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
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
	{
		typename t_CCommandLineSpecification::CInternal::CSection *pSection = fg_AutoStaticCast(this->mp_pSection);
		auto &Section = *pSection;
		auto &Internal = *(this->mp_pInternal);
		auto *pCommand = Internal.f_RegisterCommand(Section, _CommandDescription);
		pCommand->m_pActorRunCommand = fg_Construct(g_ActorFunctor / fg_Move(_fRunCommand));
		pCommand->m_Flags = _Flags;
		return typename t_CCommandLineSpecification::CCommand(this->mp_pInternal, pCommand);
	}

	template <typename t_CCommandLineSpecification>
	typename t_CCommandLineSpecification::CCommand CCommandLineSpecificationDistributedAppCustomization::TCSection<t_CCommandLineSpecification>::CSection::f_RegisterDirectCommand
		(
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJSON const &_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
	{
		typename t_CCommandLineSpecification::CInternal::CSection *pSection = fg_AutoStaticCast(this->mp_pSection);
		auto &Section = *pSection;
		auto &Internal = *(this->mp_pInternal);
		auto *pCommand = Internal.f_RegisterCommand(Section, _CommandDescription);
		pCommand->m_pDirectRunCommand = fg_Construct(fg_Move(_fRunCommand));
		pCommand->m_Flags = _Flags;
		return typename t_CCommandLineSpecification::CCommand(this->mp_pInternal, pCommand);
	}

	template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterCommand
		(
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;

	template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterDirectCommand
		(
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJSON const &_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
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
			 CStr const &_Command
			 , CEJSON const &_Params
			 , TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		co_return {};
	}

	TCFuture<uint32> CDistributedAppActor::f_RunCommandLine
		(
			CStr const &_Command
			, CEJSON const &_Params
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		if (!fp_HasCommandLineAccess(fg_GetCallingHostInfo().f_GetRealHostID()))
			co_return DMibErrorInstance("Access denied");

		auto &Internal = *mp_pInternal;

		auto &SpecInternal = Internal.m_pCommandLineSpec->f_AccessInternal();
		auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);

		if (!pCommand)
			co_return DMibErrorInstance("Non-existant command line command");

		CEJSON ValidatedParams;
		try
		{
			ValidatedParams = SpecInternal.f_ValidateParams(**pCommand, _Params);
		}
		catch (CException const &)
		{
			co_return NException::fg_CurrentException();
		}

		if ((*pCommand)->m_Flags & EDistributedAppCommandFlag_WaitForRemotes)
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_WaitForInitialConnection);

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
			co_await _pCommandLine->f_StdOut(""); // Syncronize with output
			co_return Result;
		}
		catch (CException const &)
		{
			co_return NException::fg_CurrentException();
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
}
