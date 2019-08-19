// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NCommandLine;

	namespace
	{
		CStr fg_RemoveListen(CEJSON &_Database, CStr const &_Address)
		{
			auto &Listen = _Database["Listen"];
			if (Listen.f_Type() != EJSONType_Array)
				Listen = EJSONType_Array;

			auto &ListenArray = Listen.f_Array();

			bool bDoneSomething = true;
			while (bDoneSomething)
			{
				bDoneSomething = false;
				for (auto iListen = fg_Const(ListenArray).f_GetIterator(); iListen; ++iListen)
				{
					if (iListen->f_Type() != EJSONType_Object)
						continue;

					if (auto pAddress = iListen->f_GetMember("Address", EJSONType_String))
					{
						if (pAddress->f_String() != _Address)
							continue;
						ListenArray.f_Remove(&*iListen - ListenArray.f_GetArray());
						bDoneSomething = true;
						break;
					}
				}
			}

			if (ListenArray.f_IsEmpty())
				return {};

			auto &FirstListen = ListenArray.f_GetFirst();
			if (FirstListen.f_Type() != EJSONType_Object)
				return {};

			if (auto pAddress = FirstListen.f_GetMember("Address", EJSONType_String))
				return pAddress->f_String();

			return {};
		}
		bool fg_AddListen(CEJSON &_Database, CStr const &_Address, bool _bPrimary)
		{
			auto &Listen = _Database["Listen"];
			if (Listen.f_Type() != EJSONType_Array)
				Listen = EJSONType_Array;
			auto &ListenArray = Listen.f_Array();
			if (_bPrimary)
			{
				ListenArray.f_InsertFirst(CEJSON{"Address"_= _Address});
				return true;
			}
			else
			{
				ListenArray.f_Insert(CEJSON{"Address"_= _Address});
				return ListenArray.f_GetLen() == 1;
			}
		}
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddListen(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, Address);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' from command line", _URL);

		auto EncodedURL = Address.m_URL.f_Encode();

		bool bPrimary = fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, false);

		co_await mp_State.m_ConfigDatabase.f_Save();

		if (bPrimary)
		{
			mp_PrimaryListen = Address;
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' saved to database from command line", _URL);
		}
		else
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Add listen '{}' saved to database from command line", _URL);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveListen(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, Address);
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' from command line", _URL);

		auto EncodedURL = Address.m_URL.f_Encode();

		CStr NewPrimary = fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);

		co_await mp_State.m_ConfigDatabase.f_Save();

		if (NewPrimary != mp_PrimaryListen.m_URL.f_Encode())
		{
			mp_PrimaryListen.m_URL = NewPrimary;
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database and new primary listen set to '{}' from command line", _URL, NewPrimary);
		}
		else
			DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database from command line", _URL);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetPrimaryListen(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		bool bHasListen = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, Address);
		if (!bHasListen)
			co_return DMibErrorInstance("Not listening to this address, so can't sett as primary listen.");

		auto EncodedURL = Address.m_URL.f_Encode();

		fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);
		fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, true);

		co_await mp_State.m_ConfigDatabase.f_Save();

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Set primary listen '{}' from command line", EncodedURL);
		mp_PrimaryListen = Address;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListListen(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_TableType)
	{
		TCSet<CDistributedActorTrustManager_Address> Listens = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("Address");

		for (auto &Listen : Listens)
			TableRenderer.f_AddRow(Listen.m_URL.f_Encode());

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}
}
