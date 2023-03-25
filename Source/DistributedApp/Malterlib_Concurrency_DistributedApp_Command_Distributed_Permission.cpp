// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NStr;
	using namespace NCommandLine;
	using namespace NStorage;

	namespace NPrivate
	{
		CPermissionRequirements fg_ParsePermissionRequirements(CEJSON const &_AuthenticationFactors)
		{
			CPermissionRequirements Requirements;
			if (!_AuthenticationFactors.f_IsValid())
				return Requirements;

			if (_AuthenticationFactors.f_IsString())
			{
				Requirements.m_AuthenticationFactors[TCSet<CStr>{_AuthenticationFactors.f_String()}];
				return Requirements;
			}

			for (auto &Inner : _AuthenticationFactors.f_Array())
			{
				if (Inner.f_IsArray())
				{
					TCSet<CStr> Factors;
					for (auto &Factor : Inner.f_Array())
						Factors[Factor.f_String()];
					Requirements.m_AuthenticationFactors[Factors];
				}
				else
					Requirements.m_AuthenticationFactors[TCSet<CStr>{Inner.f_String()}];
			}

			return Requirements;
		}
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListPermissions
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, bool _bIncludeTargets
			, CStr const &_TableType
		)
	{
		auto Permissions = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumPermissions, _bIncludeTargets);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();

		if (_bIncludeTargets)
			TableRenderer.f_AddHeadings("Permission", "Targets");
		else
			TableRenderer.f_AddHeadings("Permission");

		TableRenderer.f_SetMaxColumnWidth(1, 70);

		for (auto &Permission : Permissions.m_Permissions)
		{
			auto &PermissionName = Permissions.m_Permissions.fs_GetKey(Permission);
			if (_bIncludeTargets)
			{
				CStr Targets;

				for (auto &PermissionInfo : Permission)
					fg_AddStrSep(Targets, PermissionInfo.f_GetDescColored(_pCommandLine->m_AnsiFlags), "\n");

				TableRenderer.f_AddRow(PermissionName, Targets);
			}
			else
				TableRenderer.f_AddRow(PermissionName);
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported permissions to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddPermission
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_HostID
			, CStr const &_UserID
			, TCVector<CStr> const &_Permissions
			, CEJSON const &_AuthenticationFactors
			, int64 _AuthenticationLifetime
		)
	{
		if (_HostID.f_IsEmpty() && _UserID.f_IsEmpty())
			co_return DMibErrorInstance("You need to specify at least one of --host or --user");

		auto Requirements = NPrivate::fg_ParsePermissionRequirements(_AuthenticationFactors);

		TCMap<CStr, CPermissionRequirements> Permissions;
		for (auto &Permission : _Permissions)
			Permissions[Permission] = Requirements;

		if (Permissions.f_IsEmpty())
			co_return DMibErrorInstance("You need to specify at least one permission");

		co_await mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_AddPermissions
				, CPermissionIdentifiers{_HostID, _UserID}
				, Permissions
				, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
			)
		;

		DMibLogWithCategory
			(
				Mib/Concurrency/App
				, Info
				, "Add permissions '{vs}' to host '{}' user '{}' authentication factor '{}' from command line"
				, _Permissions
				, _HostID
				, _UserID
				, _AuthenticationFactors
			)
		;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemovePermission
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_HostID
			, CStr const &_UserID
			, CStr const &_Permission
		)
	{
		co_await mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_RemovePermissions
				, CPermissionIdentifiers{_HostID, _UserID}
				, TCSet<CStr>{_Permission}
				, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
			)
		;

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove permission '{}' from host '{}' user '{}' from command line", _Permission, _HostID, _UserID);

		co_return 0;
	}
}
