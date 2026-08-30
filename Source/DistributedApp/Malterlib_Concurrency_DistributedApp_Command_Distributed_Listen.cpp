// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NCommandLine;

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddListen(TCSharedPointer<CCommandLineControl> _pCommandLine, CStr _URL, bool _bPrimary, CStr _SendWindow)
	{
		uint64 SendWindowBytes = 0;
		{
			CStr Error;
			if (!fg_ParseSendWindow(_SendWindow, SendWindowBytes, Error))
				co_return DMibErrorInstance(Error);
		}

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, Address, SendWindowBytes);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Add listen '{}' from command line", _URL);

		if (!_bPrimary)
		{
			auto CurrentPrimary = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetPrimaryListen);
			if (!CurrentPrimary)
				_bPrimary = true;
			else
			{
				CDistributedActorTrustManager_Address LocalListen;
				LocalListen.m_URL = fp_GetLocalAddress();

				if (*CurrentPrimary == LocalListen)
					_bPrimary = true;
			}
		}

		if (_bPrimary)
		{
			co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetPrimaryListen, Address);
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Added listen '{}' set as primary", _URL);
		}

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetListenSendWindow(TCSharedPointer<CCommandLineControl> _pCommandLine, CStr _URL, CStr _SendWindow)
	{
		uint64 SendWindowBytes = 0;
		CStr Error;
		if (!fg_ParseSendWindow(_SendWindow, SendWindowBytes, Error))
			co_return DMibErrorInstance(Error);

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetListenSendWindow, Address, SendWindowBytes);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Change send window to {} for listen '{}' from command line", fg_FormatSendWindow(SendWindowBytes), _URL);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveListen(TCSharedPointer<CCommandLineControl> _pCommandLine, CStr _URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, Address);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' from command line", _URL);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetPrimaryListen(TCSharedPointer<CCommandLineControl> _pCommandLine, CStr _URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		bool bHasListen = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, Address);
		if (!bHasListen)
			co_return DMibErrorInstance("Not listening to this address, so can't sett as primary listen.");

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetPrimaryListen, Address);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Set primary listen '{}' from command line", _URL);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListListen(TCSharedPointer<CCommandLineControl> _pCommandLine, CStr _TableType)
	{
		auto Listens = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens);
		auto PrimaryListen = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetPrimaryListen);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("Address", "Primary", "Send Window");

		for (auto &ListenEntry : Listens.f_Entries())
		{
			auto &Address = ListenEntry.f_Key();
			auto PrimaryValue = (PrimaryListen && *PrimaryListen == Address.m_URL ? "true" : "");
			TableRenderer.f_AddRow(Address.m_URL.f_Encode(), PrimaryValue, fg_FormatSendWindow(ListenEntry.f_Value().m_SendWindowBytes));
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}
}
