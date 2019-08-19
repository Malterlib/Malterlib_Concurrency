// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NException;
	using namespace NStorage;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCommandLine;

	namespace NPrivate
	{
		CPermissionRequirements fg_ParsePermissionRequirements(CEJSON const &_AuthenticationFactors);
	}

	static CExceptionPointer fg_ValidateConnectionConcurrency(int32 _Concurrency)
	{
		if (_Concurrency == -1 || (_Concurrency >= 1 && _Concurrency <= 128))
			return nullptr;

		return DMibErrorInstance("Connection concurrency must be between 1 and 128 or -1").f_ExceptionPointer();
	}

	auto CDistributedAppActor::f_CommandLine_AddConnection
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Ticket
			, bool _bIncludeFriendlyHostName
			, int32 _ConnectionConcurrency
			, TCSet<CStr> &&_TrustedNamespaces
		)
		-> TCFuture<uint32>
	{
		for (auto &Namespace : _TrustedNamespaces)
		{
			if (!CActorDistributionManager::fs_IsValidNamespaceName(Namespace))
				co_return DMibErrorInstance("'{}' is not a valid namespace name"_f << Namespace);
		}

		if (auto pError = fg_ValidateConnectionConcurrency(_ConnectionConcurrency))
			co_return pError;

		CStrSecure TicketStr;
		if (_Ticket.f_IsEmpty())
			TicketStr = co_await _pCommandLine->f_ReadPrompt({"Please paste connection ticket: ", false});
		else
			TicketStr = _Ticket;

		CDistributedActorTrustManager::CTrustTicket Ticket;
		try
		{
			CDisableExceptionTraceScope DisableExceptionTrace;
			Ticket = CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(TicketStr);
		}
		catch (CException const &_Exception)
		{
			co_return DMibErrorInstance(fg_Format("Faild to decode ticket: {}", _Exception.f_GetErrorStr()));
		}

		CHostInfo HostInfo = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, 30.0, _ConnectionConcurrency);
		DMibLogWithCategory(Mib/Concurrency/App, Info, "Add connection to host '{}' from command line", HostInfo.f_GetDesc());

		if (!_TrustedNamespaces.f_IsEmpty())
		{
			TCActorResultMap<CStr, void> TrustResults;

			TCSet<CStr> Hosts;
			Hosts[HostInfo.m_HostID];

			CStr HostID = HostInfo.m_HostID;

			for (auto &Namespace : _TrustedNamespaces)
			{
				mp_State.m_TrustManager
					(
						&CDistributedActorTrustManager::f_AllowHostsForNamespace
						, Namespace
						, Hosts
						, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
					)
					> TrustResults.f_AddResult(Namespace);
				;
			}

			auto Results = co_await TrustResults.f_GetResults().f_Wrap();
			if (!Results)
				*_pCommandLine %= "Failed to trust namespaces: {}\n"_f << Results.f_GetExceptionStr();
			else
			{
				TCSet<CStr> SuccessfulNamespaces;
				for (auto &NamespaceResult : *Results)
				{
					CStr const &NamespaceName = Results->fs_GetKey(NamespaceResult);
					if (NamespaceResult)
						SuccessfulNamespaces[NamespaceName];
					else
						*_pCommandLine %= "Failed to trust namespace '{}': {}\n"_f << NamespaceName << NamespaceResult.f_GetExceptionStr();
				}
				if (!SuccessfulNamespaces.f_IsEmpty())
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/App
							, Info
							, "Trusted host '{}' for namespaces {vs} from add connection command line"
							, HostID
							, SuccessfulNamespaces
						)
					;
				}
			}
		}

		if (_bIncludeFriendlyHostName)
			*_pCommandLine += "{}\n"_f << HostInfo.f_GetDescColored(_pCommandLine->m_AnsiFlags);
		else
			*_pCommandLine += "{}\n"_f << HostInfo.m_HostID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GenerateTrustTicket
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_ForListen
			, TCSet<CStr> const &_Permissions
			, CStr const &_UserID
			, CEJSON const &_AuthenticationFactors
		)
	{
		CDistributedActorTrustManager_Address ForListen;
		if (_ForListen.f_IsEmpty())
		{
			if (!mp_PrimaryListen.m_URL.f_IsValid())
				co_return DMibErrorInstance("The app has not been configured to listen, so ticket cannot be generated");
			ForListen = mp_PrimaryListen;
		}
		else
			ForListen.m_URL = _ForListen;

		CStr TicketPermissionRequestID = NCryptography::fg_RandomID();

		COnScopeExitShared pCleanup;

		bool bHasPermissions = !_Permissions.f_IsEmpty();

		if (bHasPermissions)
		{
			pCleanup = g_OnScopeExitActor > [this, TicketPermissionRequestID]
				{
					mp_TicketPermissionSubscriptions.f_Remove(TicketPermissionRequestID);
				}
			;
		}

		auto Requirements = NPrivate::fg_ParsePermissionRequirements(_AuthenticationFactors);

		CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult Ticket = co_await mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, ForListen
				, nullptr
				, bHasPermissions ? g_ActorFunctor / [this, _Permissions, pCleanup = fg_Move(pCleanup), _UserID, Requirements]
				(
					CStr const &_HostID
					, CCallingHostInfo const &_HostInfo
				) mutable -> TCFuture<void>
				{
					if (_Permissions.f_IsEmpty())
					co_return {};

					TCMap<CStr, CPermissionRequirements> Permissions;
					for (auto const &Permission : _Permissions)
						Permissions[Permission] = Requirements;

					co_await mp_State.m_TrustManager
						(
							&CDistributedActorTrustManager::f_AddPermissions
							, CPermissionIdentifiers{_HostID, _UserID}
							, Permissions
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						)
					;

					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add permissions {vs} to host '{}' for user '{}' as part of ticket usage", _Permissions, _HostID, _UserID);

					co_return {};
				}
				: nullptr
			)
		;
		if (bHasPermissions)
			mp_TicketPermissionSubscriptions[TicketPermissionRequestID] = fg_Move(Ticket.m_NotificationsSubscription);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Generated trust ticket with address '{}' from command line", Ticket.m_Ticket.m_ServerAddress.m_URL.f_Encode());
		*_pCommandLine += CStrSecure::CFormat("{}\n") << Ticket.m_Ticket.f_ToStringTicket();

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetConnetionStatus(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_TableType)
	{
		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		CAnsiEncoding AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		TableRenderer.f_AddHeadings("URL", "Host", "Concurrency", "Connected", "Last error time", "Last error");
		TableRenderer.f_SetMaxColumnWidth(5, 50);

		auto ConnectionState = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetConnectionState);
		TCMap<CDistributedActorTrustManager_Address, CDistributedActorTrustManager::CAddressConnectionState> AddressState;

		for (auto &Host : ConnectionState.m_Hosts)
		{
			for (auto &State : Host.m_Addresses)
				AddressState[State.f_GetAddress()] = State;
		}

		for (auto &State : AddressState)
		{
			auto &Address = AddressState.fs_GetKey(State);

			CDistributedActorTrustManager::CConcurrentConnectionState *pFirstState = nullptr;
			if (!State.m_States.f_IsEmpty())
				pFirstState = &State.m_States.f_GetFirst();

			TCVector<CStr> Connected;
			TCVector<CStr> ErrorTime;
			TCVector<CStr> Error;

			for (auto &ConcurrentState : State.m_States)
			{
				if (ConcurrentState.m_bConnected)
				{
					Connected.f_Insert(AnsiEncoding.f_StatusNormal("Yes"));
					if (ConcurrentState.m_Error.f_IsEmpty())
						Error.f_Insert("None");
					else
						Error.f_Insert(AnsiEncoding.f_Bold("Error: ") + ConcurrentState.m_Error);
				}
				else
				{
					Connected.f_Insert(AnsiEncoding.f_StatusError("NO"));
					if (ConcurrentState.m_Error.f_IsEmpty())
						Error.f_Insert("None");
					else
						Error.f_Insert(AnsiEncoding.f_StatusError("Error: ") + ConcurrentState.m_Error);
				}
				ErrorTime.f_Insert(ConcurrentState.m_ErrorTime.f_IsValid() ? fg_Format("{}", ConcurrentState.m_ErrorTime) : "Never");
			}

			if (State.m_States.f_IsEmpty())
			{
				Connected.f_Insert("NO");
				ErrorTime.f_Insert("");
				Error.f_Insert("");
			}

			TableRenderer.f_AddRow
				(
					Address.m_URL.f_Encode()
					, State.m_HostInfo.f_GetDescColored(_pCommandLine->m_AnsiFlags)
					, CStr::fs_ToStr(State.m_States.f_GetLen())
					, CStr::fs_Join(Connected, "\n")
					, CStr::fs_Join(ErrorTime, "\n")
					, CStr::fs_Join(Error, "\n")
				)
			;
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported connection status to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListConnections
		(
		 	TCSharedPointer<CCommandLineControl> const &_pCommandLine
		 	, bool _bIncludeFriendlyHostName
		 	, CStr const &_TableType
		)
	{
		auto ClientConnections = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("URL", "Host", "Concurrency");

		for (auto &ConnectionInfo : ClientConnections)
		{
			auto &Address = ClientConnections.fs_GetKey(ConnectionInfo);

			CStr HostInfo;
			if (_bIncludeFriendlyHostName)
				HostInfo = ConnectionInfo.m_HostInfo.f_GetDescColored(_pCommandLine->m_AnsiFlags);
			else
				HostInfo = ConnectionInfo.m_HostInfo.m_HostID;

			TableRenderer.f_AddRow(Address.m_URL.f_Encode(), HostInfo, ConnectionInfo.m_ConnectionConcurrency);
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported connections to command line");

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveConnection(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_URL)
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, Address, false);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove connection '{}' from command line", _URL);

		co_return 0;
	}

	auto CDistributedAppActor::f_CommandLine_AddAdditionalConnection
		(
			 TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 , CStr const &_URL
			 , bool _bIncludeFriendlyHostName
			 , int32 _ConnectionConcurrency
		)
		-> TCFuture<uint32>
	{
		if (auto pError = fg_ValidateConnectionConcurrency(_ConnectionConcurrency))
			co_return pError;

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		CHostInfo HostInfo = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, Address, _ConnectionConcurrency);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Add additional connection to host '{}' from command line", HostInfo.f_GetDesc());
		if (_bIncludeFriendlyHostName)
			*_pCommandLine += "{}\n"_f << HostInfo.f_GetDescColored(_pCommandLine->m_AnsiFlags);
		else
			*_pCommandLine += "{}\n"_f << HostInfo.m_HostID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetConnectionConcurrency
		(
			 TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 , CStr const &_URL
			 , int32 _ConnectionConcurrency
		)
	{
		if (auto pError = fg_ValidateConnectionConcurrency(_ConnectionConcurrency))
			co_return pError;

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, Address, _ConnectionConcurrency);

		DMibLogWithCategory
			(
				Mib/Concurrency/App
				, Info
				, "Change connection concurrency to {} for address '{}' from command line"
				, _ConnectionConcurrency
				, Address.m_URL.f_Encode()
			)
		;

		co_return 0;
	}
}
