// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCommandLine;

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetHostID(TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		CStr HostID = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported host id '{}' to command line", HostID);

		*_pCommandLine += "{}\n"_f << HostID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListTrustedHosts(TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName, CStr const &_TableType)
	{
		TCMap<CStr, CHostInfo> TrustedHosts = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClients);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("Host");

		for (auto &Host : TrustedHosts)
		{
			if (_bIncludeFriendlyHostName)
				TableRenderer.f_AddRow(Host.f_GetDescColored(_pCommandLine->m_AnsiFlags));
			else
				TableRenderer.f_AddRow(Host.m_HostID);
		}
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported trusted hosts to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveTrustedHost(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_HostID)
	{
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove trusted host '{}' from command line", _HostID);

		co_return 0;
	}
}
