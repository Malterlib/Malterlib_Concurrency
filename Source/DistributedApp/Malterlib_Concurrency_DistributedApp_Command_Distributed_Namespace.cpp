// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NStorage;
	using namespace NCommandLine;
	using namespace NContainer;

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListNamespaces
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, bool _bIncludeTrustedHosts
			, CStr _TableType
		)
	{

		auto Namespaces = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeTrustedHosts);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		CAnsiEncoding AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		if (_bIncludeTrustedHosts)
			TableRenderer.f_AddHeadings("Namespace", "Trusted", "Untrusted");
		else
			TableRenderer.f_AddHeadings("Namespace");

		for (auto &Permissions : Namespaces)
		{
			auto &Namespace = Namespaces.fs_GetKey(Permissions);

			if (_bIncludeTrustedHosts)
			{
				CStr Trusted;
				CStr Untrusted;

				for (auto &Host : Permissions.m_AllowedHosts)
					fg_AddStrSep(Trusted, Host.f_GetDescColored(_pCommandLine->m_AnsiFlags), "\n");

				for (auto &Host : Permissions.m_DisallowedHosts)
				{
					TCVector<CStr> HostDesc = Host.f_GetDescColored(_pCommandLine->m_AnsiFlags).f_Split<true>(" ");
					if (!HostDesc.f_IsEmpty())
						HostDesc[0] = AnsiEncoding.f_StatusWarning(HostDesc[0]);
					fg_AddStrSep(Untrusted, CStr::fs_Join(HostDesc, " "), "\n");
				}

				TableRenderer.f_AddRow(Namespace, Trusted, Untrusted);
			}
			else
				TableRenderer.f_AddRow(Namespace);
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported namespaces to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_TrustHostForNamespace
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, CStr _Namespace
			, CStr _Host
		)
	{
		TCSet<CStr> Hosts;
		Hosts[_Host];
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Trusted host '{}' for namespace '{}' from command line", _Host, _Namespace);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_UntrustHostForNamespace
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, CStr _Namespace
			, CStr _Host
		)
	{
		TCSet<CStr> Hosts;
		Hosts[_Host];
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Untrusted host '{}' for namespace '{}' from command line", _Host, _Namespace);

		co_return 0;
	}
}
