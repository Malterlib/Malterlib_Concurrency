// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib
{
	using namespace NStr;
	using namespace NFile;
	using namespace NPtr;
	using namespace NEncoding;
	using namespace NContainer;
	
	namespace NConcurrency
	{
		ICCommandLine::ICCommandLine()
		{
			DMibPublishActorFunction(ICCommandLine::f_RunCommandLine);
		}

		CDistributedAppCommandLineResults::COutput::COutput() = default;
		
		CDistributedAppCommandLineResults::COutput::COutput(NStr::CStr const &_Output, EOutputType _OutputType)
			: m_OutputType(_OutputType)
			, m_Output(_Output)
		{
		}

		CDistributedAppCommandLineResults::CDistributedAppCommandLineResults() = default;

		CDistributedAppCommandLineResults::CDistributedAppCommandLineResults(CAsyncResult const &_Result)
		{
			f_AddAsyncResult(_Result);
		}
		
		CDistributedAppCommandLineResults::CDistributedAppCommandLineResults(NStr::CStr const &_Output, EOutputType _OutputType)
		{
			m_Output.f_Insert(fg_Construct(_Output, _OutputType));
		}																			 

		void CDistributedAppCommandLineResults::f_AddAsyncResult(CAsyncResult const &_Result)
		{
			if (!_Result)
			{
				f_AddStdErr(fg_Format("{}\n", _Result.f_GetExceptionStr()));
				m_Status = 1;
			}
		}
		
		void CDistributedAppCommandLineResults::f_AddStdOut(CStr const &_Output)
		{
			m_Output.f_Insert(fg_Construct(_Output, EOutputType_StdOut));
		}
		
		void CDistributedAppCommandLineResults::f_AddStdErr(CStr const &_Output)
		{
			m_Output.f_Insert(fg_Construct(_Output, EOutputType_StdErr));
		}
		
		void CDistributedAppCommandLineResults::f_SetExitStatus(uint32 _Status)
		{
			m_Status = _Status;
		}

		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::fp_PreRunCommandLine(CStr const &_Command, NEncoding::CEJSON const &_Params)
		{
			return fg_Explicit(CDistributedAppCommandLineResults());
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_RunCommandLine
			(
				CCallingHostInfo const &_CallingHost
				, NStr::CStr const &_Command
				, NEncoding::CEJSON const &_Params
			)
		{
			if (!fp_HasCommandLineAccess(_CallingHost.f_GetRealHostID()))
				return DMibErrorInstance("Access denied");

			auto &SpecInternal = *(mp_pCommandLineSpec->mp_pInternal);
			auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
			
			if (!pCommand)
				return DMibErrorInstance("Non-existant command line command");

			CEJSON ValidatedParams;
			try
			{
				ValidatedParams = SpecInternal.f_ValidateParams(**pCommand, _Params);
			}
			catch (NException::CException const &_Exception)
			{
				return _Exception;
			}
			
			CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};
		
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			fp_PreRunCommandLine(_Command, ValidatedParams) > Continuation / [this, Continuation, ValidatedParams, _Command, _CallingHost](CDistributedAppCommandLineResults &&_PreResults)
				{
					auto &SpecInternal = *(mp_pCommandLineSpec->mp_pInternal);
					
					auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
					if (!pCommand)
					{
						Continuation.f_SetException(DMibErrorInstance("Non-existant command line command"));
						return;
					}
					
					auto &Command = **pCommand;
					if (!Command.m_fActorRunCommand)
					{
						Continuation.f_SetException(DMibErrorInstance("Non-actor cammand"));
						return;
					}

					try
					{
						CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};
						Command.m_fActorRunCommand(ValidatedParams) > Continuation / [this, _PreResults, Continuation](CDistributedAppCommandLineResults &&_Results)
							{
								auto FinalResults = _PreResults;
								FinalResults.m_Status = _Results.m_Status;
								FinalResults.m_Output.f_Insert(fg_Move(_Results.m_Output));
								Continuation.f_SetResult(fg_Move(FinalResults));
							}
						;
					}
					catch (NException::CException const &_Exception)
					{
						Continuation.f_SetException(_Exception);
						return;
					}
				}
			;
			
			return Continuation;
		}
		
		static bool fg_ValidateConnectionConcurrency(int32 _Concurrency, TCContinuation<CDistributedAppCommandLineResults> &o_Continuation)
		{
			if (_Concurrency == -1 || (_Concurrency >= 1 && _Concurrency <= 128))
				return true;
			
			o_Continuation.f_SetException(DMibErrorInstance("Connection concurrency must be between 1 and 128 or -1"));
			return false;
		}
		
		auto CDistributedAppActor::f_CommandLine_AddConnection(NStr::CStr const &_Ticket, bool _bIncludeFriendlyHostName, int32 _ConnectionConcurrency)
			-> TCContinuation<CDistributedAppCommandLineResults> 
		{
			CDistributedActorTrustManager::CTrustTicket Ticket;
			try
			{
				Ticket = CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(_Ticket);
			}
			catch (NException::CException const &_Exception)
			{
				return DMibErrorInstance(fg_Format("Faild to decode ticket: {}", _Exception.f_GetErrorStr()));
			}
			
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, 30.0, _ConnectionConcurrency)
				> Continuation / [Continuation, _bIncludeFriendlyHostName](CHostInfo &&_HostInfo)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add connection to host '{}' from command line", _HostInfo.f_GetDesc());
					if (_bIncludeFriendlyHostName)
						Continuation.f_SetResult(fg_Format("{}\n", _HostInfo.f_GetDesc()));
					else
						Continuation.f_SetResult(fg_Format("{}\n", _HostInfo.m_HostID));
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_GenerateTrustTicket(CStr const &_ForListen)
		{
			CDistributedActorTrustManager_Address ForListen;
			if (_ForListen.f_IsEmpty())
			{
				if (!mp_PrimaryListen.m_URL.f_IsValid())
					return DMibErrorInstance("The app has not been configured to listen, so ticket cannot be generated");
				ForListen = mp_PrimaryListen;
			}
			else
				ForListen.m_URL = _ForListen;
				
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ForListen, nullptr) 
				> Continuation / [Continuation](CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Generated trust ticket with address '{}' from command line", _Ticket.m_Ticket.m_ServerAddress.m_URL.f_Encode());
					Continuation.f_SetResult(fg_Format("{}\n", _Ticket.m_Ticket.f_ToStringTicket()));
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_GetHostID()
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID) 
				> Continuation / [Continuation](CStr &&_HostID)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported host id '{}' to command line", _HostID);
					Continuation.f_SetResult(fg_Format("{}\n", _HostID));
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_ListTrustedHosts(bool _bIncludeFriendlyHostName)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClients)
				> Continuation / [Continuation, _bIncludeFriendlyHostName](NContainer::TCMap<NStr::CStr, CHostInfo> &&_TrustedHosts)
				{
					CStr Result;
					for (auto &Host : _TrustedHosts)
					{
						if (_bIncludeFriendlyHostName)
							Result += fg_Format("{}\n", Host.f_GetDesc());
						else
							Result += fg_Format("{}\n", Host.m_HostID);
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported trusted hosts to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_RemoveTrustedHost(NStr::CStr const &_HostID)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID)
				> Continuation / [Continuation, _HostID]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove trusted host '{}' from command line", _HostID);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_GetConnetionStatus()
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetConnectionState)
				> Continuation / [Continuation](CDistributedActorTrustManager::CConnectionState &&_ConnectionState)
				{
					mint LongestAddress = fg_StrLen("URL");
					mint LongestHostInfo = fg_StrLen("Host");
					TCMap<CDistributedActorTrustManager_Address, CDistributedActorTrustManager::CAddressConnectionState> AddressState;
					for (auto &Host : _ConnectionState.m_Hosts)
					{
						for (auto &State : Host.m_Addresses)
						{
							LongestAddress = fg_Max(State.f_GetAddress().m_URL.f_Encode().f_GetLen(), aint(LongestAddress));
							LongestHostInfo = fg_Max(State.m_HostInfo.f_GetDesc().f_GetLen(), aint(LongestHostInfo));
							AddressState[State.f_GetAddress()] = State;
						}
					}
					CStr Result;
					auto fOutputLine = [&](CStr const &_URL, CStr const &_Connected, CStr const &_HostInfo, CStr const &_Concurrency, auto const &_ErrorTime, CStr const &_Error)
						{
							Result += fg_Format
								(
									"{sj*,a-}     {sj*,a-}     {sj9,a-}     {sj11,a-}     {sj23,a-}     {a-}\n"
									, _URL
									, LongestAddress
									, _HostInfo
									, LongestHostInfo
									, _Connected
									, _Concurrency
									, _ErrorTime
									, _Error
								)
							;
						}
					;
					fOutputLine("URL", "Connected", "Host", "Concurrency", "Last error time", "Last error");
					for (auto &State : AddressState)
					{
						auto &Address = AddressState.fs_GetKey(State);
						
						CDistributedActorTrustManager::CConcurrentConnectionState *pFirstState = nullptr;
						if (!State.m_States.f_IsEmpty())
							pFirstState = &State.m_States.f_GetFirst(); 
							
						fOutputLine
							(
								Address.m_URL.f_Encode()
								, pFirstState ? (pFirstState->m_bConnected ? "Yes" : "NO") : "NO"
								, State.m_HostInfo.f_GetDesc()
								, CStr::fs_ToStr(State.m_States.f_GetLen()) 
								, pFirstState ? (pFirstState->m_ErrorTime.f_IsValid() ? fg_Format("{}", pFirstState->m_ErrorTime) : "Never") : ""
								, pFirstState ? (!pFirstState->m_Error.f_IsEmpty() ? pFirstState->m_Error : "None") : ""
							)
						;
						
						auto iConcurentState = State.m_States.f_GetIterator();
						if (iConcurentState)
							++iConcurentState;
							
						for (; iConcurentState; ++iConcurentState)
						{
							auto &ConcurrentState = *iConcurentState;
							fOutputLine
								(
									""
									, ConcurrentState.m_bConnected ? "Yes" : "NO"
									, ""
									, "" 
									, ConcurrentState.m_ErrorTime.f_IsValid() ? fg_Format("{}", ConcurrentState.m_ErrorTime) : "Never"
									, !ConcurrentState.m_Error.f_IsEmpty() ? ConcurrentState.m_Error : "None"
								)
							;
						}
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported connection status to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}
		 
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_ListConnections(bool _bIncludeFriendlyHostName)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections)
				> Continuation / [Continuation, _bIncludeFriendlyHostName]
				(NContainer::TCMap<CDistributedActorTrustManager_Address, CDistributedActorTrustManager::CClientConnectionInfo> &&_ClientConnections)
				{
					CStr Result;
					for (auto &ConnectionInfo : _ClientConnections)
					{
						auto &Address = _ClientConnections.fs_GetKey(ConnectionInfo);
						
						CStr HostInfo;
						if (_bIncludeFriendlyHostName)
							HostInfo = ConnectionInfo.m_HostInfo.f_GetDesc();
						else
							HostInfo = ConnectionInfo.m_HostInfo.m_HostID;

						Result += fg_Format("{}     {}     {}\n", Address.m_URL.f_Encode(), HostInfo, ConnectionInfo.m_ConnectionConcurrency);
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported connections to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_RemoveConnection(NStr::CStr const &_URL)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, Address)
				> Continuation / [Continuation, _URL]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove connection '{}' from command line", _URL);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}
		
		auto CDistributedAppActor::f_CommandLine_AddAdditionalConnection(NStr::CStr const &_URL, bool _bIncludeFriendlyHostName, int32 _ConnectionConcurrency)
			-> TCContinuation<CDistributedAppCommandLineResults> 
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, Address, _ConnectionConcurrency)
				> Continuation / [Continuation, _bIncludeFriendlyHostName](CHostInfo &&_HostInfo)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add additional connection to host '{}' from command line", _HostInfo.f_GetDesc());
					if (_bIncludeFriendlyHostName)
						Continuation.f_SetResult(fg_Format("{}\n", _HostInfo.f_GetDesc()));
					else
						Continuation.f_SetResult(fg_Format("{}\n", _HostInfo.m_HostID));
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_SetConnectionConcurrency(NStr::CStr const &_URL, int32 _ConnectionConcurrency)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, Address, _ConnectionConcurrency)
				> Continuation / [Continuation, Address, _ConnectionConcurrency]()
				{
					DMibLogWithCategory
						(
							Mib/Concurrency/App
							, Info
							, "Change connection concurrency to {} for address '{}' from command line"
							, _ConnectionConcurrency
							, Address.m_URL.f_Encode()
						)
					;
					Continuation.f_SetResult(CDistributedAppCommandLineResults());
				}
			;
			return Continuation;
		}
		
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
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_AddListen(NStr::CStr const &_URL)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, Address) 
				> Continuation / [this, Continuation, _URL, Address]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' from command line", _URL);
					
					auto EncodedURL = Address.m_URL.f_Encode();
					
					bool bPrimary = fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, false);
					
					mp_State.m_ConfigDatabase.f_Save() > Continuation / [this, Continuation, Address, bPrimary, _URL]
						{
							if (bPrimary)
							{
								mp_PrimaryListen = Address;
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' saved to database from command line", _URL);
							}
							else
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Add listen '{}' saved to database from command line", _URL);
							Continuation.f_SetResult(CStr());
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_RemoveListen(NStr::CStr const &_URL)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, Address) 
				> Continuation / [this, Continuation, _URL, Address]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' from command line", _URL);
					
					auto EncodedURL = Address.m_URL.f_Encode();
					
					CStr NewPrimary = fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);
					
					mp_State.m_ConfigDatabase.f_Save() > Continuation / [this, Continuation, Address, NewPrimary, _URL]
						{
							if (NewPrimary != mp_PrimaryListen.m_URL.f_Encode())
							{
								mp_PrimaryListen.m_URL = NewPrimary;
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database and new primary listen set to '{}' from command line", _URL, NewPrimary);
							}
							else
								DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database from command line", _URL);
							Continuation.f_SetResult(CStr());
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_SetPrimaryListen(NStr::CStr const &_URL)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, Address) 
				> Continuation / [this, Continuation, _URL, Address](bool _bHasListen)
				{
					if (!_bHasListen)
					{
						Continuation.f_SetException(DMibErrorInstance("Not listening to this address, so can't sett as primary listen."));
						return;
					}

					auto EncodedURL = Address.m_URL.f_Encode();
					
					fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);
					fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, true);
					
					mp_State.m_ConfigDatabase.f_Save() > Continuation / [this, Continuation, Address, EncodedURL]
						{
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Set primary listen '{}' from command line", EncodedURL);
							mp_PrimaryListen = Address;
							Continuation.f_SetResult(CStr());
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_ListListen()
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens)
				> Continuation / [Continuation](NContainer::TCSet<CDistributedActorTrustManager_Address> &&_Listens)
				{
					CStr Result;
					for (auto &Listen : _Listens)
						Result += fg_Format("{}\n", Listen.m_URL.f_Encode());
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_ListNamespaces(bool _bIncludeTrustedHosts)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeTrustedHosts)
				> Continuation / [Continuation, _bIncludeTrustedHosts](NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> &&_Namespaces)
				{
					CStr Result;
					for (auto &Permissions : _Namespaces)
					{
						auto &Namespace = _Namespaces.fs_GetKey(Permissions);
						Result += fg_Format("{}\n", Namespace);
						if (_bIncludeTrustedHosts)
						{
							Result += "    Trusted\n";
							for (auto &Host : Permissions.m_AllowedHosts)
								Result += fg_Format("        {}\n", Host.f_GetDesc());
							Result += "    Untrusted\n";
							for (auto &Host : Permissions.m_DisallowedHosts)
								Result += fg_Format("        {}\n", Host.f_GetDesc());
						}
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported namespaces to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}

		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_TrustHostForNamespace
			(
				NStr::CStr const &_Namespace
				, NStr::CStr const &_Host
			)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			NContainer::TCSet<NStr::CStr> Hosts;
			Hosts[_Host];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, Hosts)
				> Continuation / [Continuation, _Namespace, _Host]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Trusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_UntrustHostForNamespace
			(
				NStr::CStr const &_Namespace
				, NStr::CStr const &_Host
			)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			NContainer::TCSet<NStr::CStr> Hosts;
			Hosts[_Host];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, Hosts)
				> Continuation / [Continuation, _Namespace, _Host]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Untrusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}


		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_ListHostPermissions(bool _bIncludeHosts)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, _bIncludeHosts)
				> Continuation / [Continuation, _bIncludeHosts](NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> &&_Permissions)
				{
					CStr Result;
					for (auto &Permission : _Permissions)
					{
						auto &PermissionName = _Permissions.fs_GetKey(Permission);
						Result += fg_Format("{}\n", PermissionName);
						if (_bIncludeHosts)
						{
							for (auto &Host : Permission)
								Result += fg_Format("    {}\n", Host.f_GetDesc());
						}
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported permissions to command line");
					Continuation.f_SetResult(Result);
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_AddHostPermission(NStr::CStr const &_HostID, NStr::CStr const &_Permission)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			NContainer::TCSet<NStr::CStr> Permissions;
			Permissions[_Permission];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddHostPermissions, _HostID, Permissions)
				> Continuation / [Continuation, _HostID, _Permission]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add permission '{}' to host '{}' from command line", _Permission, _HostID);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_RemoveHostPermission(NStr::CStr const &_HostID, NStr::CStr const &_Permission)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			NContainer::TCSet<NStr::CStr> Permissions;
			Permissions[_Permission];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveHostPermissions, _HostID, Permissions)
				> Continuation / [Continuation, _HostID, _Permission]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove permission '{}' from host '{}' from command line", _Permission, _HostID);
					Continuation.f_SetResult(CStr());
				}
			;
			return Continuation;
		}

		uint32 CDistributedAppActor::f_CommandLine_RemoveAllTrust(bool _bConfirm)
		{
			CStr TrustDatabaseLocation = fg_Format("{}/TrustDatabase.{}", mp_Settings.m_ConfigDirectory, mp_Settings.m_AppName);
			CStr TrustDatabaseCommandLineLocation = fg_Format("{}/CommandLineTrustDatabase.{}", mp_Settings.m_ConfigDirectory, mp_Settings.m_AppName);
			
			auto fLogAction = [&](CStr const &_ToLog)
				{
					if (_bConfirm)
						DMibConOut("{cc}{\n}", _ToLog);
					else
						DMibConOut("Would {}{\n}", _ToLog);
				}
			;
			
			auto fClearDatabase = [&](CStr const &_Root)
				{
					CStr BasicConfigFile = CFile::fs_AppendPath(_Root, "BasicConfig.json");
					if (CFile::fs_FileExists(BasicConfigFile))
					{
						auto BasicConfig = CEJSON::fs_FromString(CFile::fs_ReadStringFromFile(BasicConfigFile), BasicConfigFile);
						BasicConfig.f_RemoveMember("CAPrivateKey");
						BasicConfig.f_RemoveMember("CACertificate");
						if (_bConfirm)
							CFile::fs_WriteStringToFile(BasicConfigFile, BasicConfig.f_ToString());
						fLogAction(fg_Format("write '{}': {}", BasicConfigFile, BasicConfig.f_ToString(nullptr)));
					}
					CStr ClientConnections = CFile::fs_AppendPath(_Root, "ClientConnections");
					if (CFile::fs_FileExists(ClientConnections))
					{
						if (_bConfirm)
							CFile::fs_DeleteDirectoryRecursive(ClientConnections);
						fLogAction(fg_Format("delete directory '{}'", ClientConnections));
					}
					CStr ListenConfigs = CFile::fs_AppendPath(_Root, "ListenConfigs");
					if (CFile::fs_FileExists(ListenConfigs))
					{
						if (_bConfirm)
							CFile::fs_DeleteDirectoryRecursive(ListenConfigs);
						fLogAction(fg_Format("delete directory '{}'", ListenConfigs));
					}
					CStr Clients = CFile::fs_AppendPath(_Root, "Clients");
					if (CFile::fs_FileExists(Clients))
					{
						if (_bConfirm)
							CFile::fs_DeleteDirectoryRecursive(Clients);
						fLogAction(fg_Format("delete directory '{}'", Clients));
					}
					CStr ServerCertificates = CFile::fs_AppendPath(_Root, "ServerCertificates");
					if (CFile::fs_FileExists(ServerCertificates))
					{
						if (_bConfirm)
							CFile::fs_DeleteDirectoryRecursive(ServerCertificates);
						fLogAction(fg_Format("delete directory '{}'", ServerCertificates));
					}
				}
			;
			
			fClearDatabase(TrustDatabaseLocation);
			if (CFile::fs_FileExists(TrustDatabaseCommandLineLocation))
			{
				if (_bConfirm)
					CFile::fs_DeleteDirectoryRecursive(TrustDatabaseCommandLineLocation);
				fLogAction(fg_Format("delete directory '{}'", TrustDatabaseCommandLineLocation));
			}
			
			if (!_bConfirm)
			{
				DMibConErrOut2("{\n}No actions were performed.{\n}{\n}");
				DMibConErrOut2("Are you really sure you want to delete all certificates, client connections and clients?{\n}{\n}");
				DMibConErrOut2("Confirm with --yes{\n}{\n}");
			}
			
			return 0;
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::fp_RunCommandLineAndLogError
			(
				CStr const &_Description
				, NFunction::TCFunction<TCContinuation<CDistributedAppCommandLineResults> ()> &&_fCommand
			)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			fg_Dispatch
				(
					[this, fCommand = fg_Move(_fCommand)]
					{
						return fCommand();
					}
				)
				> [Continuation, _Description](TCAsyncResult<CDistributedAppCommandLineResults> &&_Other)
				{
					if (!_Other)
						DMibLogWithCategory(Mib/Concurrency/App, Error, "{} failed from command line: {}", _Description, _Other.f_GetExceptionStr());
					Continuation.f_SetResult(fg_Move(_Other));
				}
			;
			
			return Continuation;
		}
		
		void CDistributedAppActor::fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine)
		{
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"ConcurrentLogging?"_=
						{
							"Names"_= {"--log-on-thread"}
							, "Default"_= true
							, "Description"_= "Log on a separate thread to minimally affect application performance and behavior"
						}
						, "StdErrLogger?"_=
						{
							"Names"_= {"--log-to-stderr"}
							, "Default"_= false
							, "Description"_= "Log to std error"
						}
					}
				)
			;
#if DMibEnableTrace > 0
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"TraceLogger?"_=
						{
							"Names"_= {"--log-to-trace"}
							, "Default"_= bool(NSys::fg_System_BeingDebugged())
							, "Description"_= "Log to trace"
						}
					}
				)
			;
#endif
			{
				auto Distributed = o_CommandLine.f_AddSection("Distributed Computing", "Use these commands to manage connectability and trust between different hosts.");
				auto IncludeFriendlyNameOption = "IncludeFriendlyName?"_= 
					{
						"Names"_= {"--include-friendly-name"}
						, "Default"_= true
						, "Description"_= "Include friendly host name in output"
					}
				;
				auto ConnectionConcurrencyOption = "ConnectionConcurrency?"_= 
					{
						"Names"_= {"--connection-concurrency"}
						, "Default"_= -1
						, "Description"_= "The number of parallel connections to connect to address with. Useful for increasing network throughput. Set to -1 to use default."
					}
				;
				
				CStr Category = "Outgoing connections";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-list"}
							, "Category"_= Category
							, "Description"_= "List the outgoing addresses that the application tries to connect to.\n"
							, "Output"_= "A new line separated list of outgoing connections."
							, "Options"_=
							{
								IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_ListConnections(_Params["IncludeFriendlyName"].f_Boolean());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-status"}
							, "Category"_= Category
							, "Description"_= "List the status for all outgoing connections.\n"
							, "Output"_= "A table with the status for outgoing connections."
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_GetConnetionStatus();
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-add"}
							, "Category"_= Category
							, "Description"_= "Add a connection to a remote distributed actor host.\n"
								"The ticket encodes the url of the remote host and can usually be generated by the remote host by running --generate-connection-ticket.\n"
								"A ticket can only be used once and expires after a set time.\n"
							, "Output"_= "The host information for the remote server."
							, "Parameters"_=
							{
								"Ticket"_= 
								{
									"Default"_= ""
									, "Description"_= "Specify ticket to use for adding the connection"
								}
							}
							, "Options"_=
							{
								ConnectionConcurrencyOption
								, IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add trust connection"
									, [this, _Params]
									{
										return f_CommandLine_AddConnection
											(
												_Params["Ticket"].f_String()
												, _Params["IncludeFriendlyName"].f_Boolean()
												, _Params["ConnectionConcurrency"].f_Integer()
											)
										;
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-set-concurrency"}
							, "Category"_= Category
							, "Description"_= "Sets the number of parallel connections to connect to address with. Useful for increasing network throughput. Set to -1 to use default.\n"
							, "Parameters"_=
							{
								"ConnectionConcurrency"_= 
								{
									"Type"_= -1
									, "Description"_= "Specify ticket to use for adding the connection"
								}
							}
							, "Options"_=
							{
								"Address"_= 
								{
									"Names"_= {"--address"}
									, "Type"_= ""
									, "Description"_= "The client connection address to set connection concurrency for."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Set connection concurrency"
									, [this, _Params]
									{
										return f_CommandLine_SetConnectionConcurrency
											(
												_Params["Address"].f_String()
												, _Params["ConnectionConcurrency"].f_Integer()
											)
										;
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-add-additional"}
							, "Category"_= Category
							, "Description"_= "Add a connection to an already trusted host.\n"
							, "Output"_= "The host information for the remote server."
							, "Parameters"_=
							{
								"ConnectionURL"_= 
								{
									"Type"_= ""
									, "Description"_= "The URL of the connection you want to add"
								}
							}
							, "Options"_=
							{
								ConnectionConcurrencyOption
								, IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add additional trust connection"
									, [this, _Params]
									{
										return f_CommandLine_AddAdditionalConnection
											(
												_Params["ConnectionURL"].f_String()
												, _Params["IncludeFriendlyName"].f_Boolean()
												, _Params["ConnectionConcurrency"].f_Integer()
											)
										;
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-connection-remove"}
							, "Category"_= Category
							, "Description"_= "Remove an outgoing connection\n"
							, "Parameters"_=
							{
								"ConnectionURL"_= 
								{
									"Type"_= ""
									, "Description"_= "The URL of the connection you want to remove"
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove trust connection"
									, [this, _Params]
									{
										return f_CommandLine_RemoveConnection(_Params["ConnectionURL"].f_String());
									}
								)
							;
						}
					)
				;
				Category = "Incoming connections trust";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-trusted-list"}
							, "Category"_= Category
							, "Description"_= "List the incoming hosts that are trusted by this application.\n"
							, "Output"_= "A line separated list of trusted connections."
							, "Options"_=
							{
								IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_ListTrustedHosts(_Params["IncludeFriendlyName"].f_Boolean());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-trusted-remove"}
							, "Category"_= Category
							, "Description"_= "Remove the incoming trust for a host id.\n"
							, "Parameters"_=
							{
								"HostID"_= 
								{
									"Type"_= ""
									, "Description"_= "The host ID of the host you want to remove trust for"
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove trusted host"
									, [this, _Params]
									{
										return f_CommandLine_RemoveTrustedHost(_Params["HostID"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-generate-ticket"}
							, "Category"_= Category
							, "Description"_= "Generate a ticket that can be used to connect to this application from remote host.\n"
								" The ticket encodes the url of this host."
								" A ticket can only be used once and expires after a set time."
								" Once used with --trust-add-connection on the remote host the remote host will be trusted by this application."
							, "Output"_= "The trust ticket that can be used with --trust-add-connection on the remote application."
							, "Parameters"_=
							{
								"ForListenURL?"_= 
								{
									"Default"_= ""
									, "Description"_=  "Specify the URL you want to generate a ticket for.\n"
										"This is needed because the ticket includes the public certificate to connect to this server. Every host has a different certificate" 
										" so you need to specify the listen address to be able to know which certificate to send with the ticket.\n"
										"If you do not specify the URL the primary listen URL will be used. The primary listen URL is the first listen URL specified in the config file."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Generate trust ticket"
									, [this, _Params]
									{
										return f_CommandLine_GenerateTrustTicket(_Params["ForListenURL"].f_String());
									}
								)
							;
						}
					)
				;
				Category = "Incoming connections listen";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-listen-list"}
							, "Category"_= Category
							, "Description"_= "List addresses that this application listens for incoming connections on.\n"
							, "Output"_= "A new line separated list of listen addresses."
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_ListListen();
						}
					)
				;
				
				CStr OverwriteWarning = fg_Format("WARNING: This will overwrite the '{}' config file and strip any comments.\n", mp_State.m_ConfigDatabase.f_GetFileName());
				
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-listen-add"}
							, "Category"_= Category
							, "Description"_= "Start listening for connections on address.\n"
								+ OverwriteWarning
							, "Parameters"_=
							{
								"ListenURL"_= 
								{
									"Type"_= ""
									, "Description"_= "The address you want to start listening on."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add trust listen address"
									, [this, _Params]
									{
										return f_CommandLine_AddListen(_Params["ListenURL"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-listen-remove"}
							, "Category"_= Category
							, "Description"_= "Stop listening for connections on address.\n"
								+ OverwriteWarning
							, "Parameters"_=
							{
								"ListenURL"_= 
								{
									"Type"_= ""
									, "Description"_= "The address you want to stop listening on."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove listen address"
									, [this, _Params]
									{
										return f_CommandLine_RemoveListen(_Params["ListenURL"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-listen-set-primary"}
							, "Category"_= Category
							, "Description"_= "Set the primary listen URL.\n"
								"This URL is the URL that will be used by default when --trust-generate-ticket is called without specifying the URL.\n"
								+ OverwriteWarning
							, "Parameters"_=
							{
								"ListenURL"_= 
								{
									"Type"_= ""
									, "Description"_= "The address you want to use as the primary listen address."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Set primary listen address"
									, [this, _Params]
									{
										return f_CommandLine_SetPrimaryListen(_Params["ListenURL"].f_String());
									}
								)
							;
						}
					)
				;
				Category = "Subscription trust";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-namespace-list"}
							, "Category"_= Category
							, "Description"_= "List actor publication namespaces that this application publishes as well as the hosts that are trusted for those namespaces.\n"
							, "Output"_= "A new line separated list of namespaces and the hosts that are trusted and untrusted."
							, "Options"_=
							{
								"IncludeTrustedHosts?"_= 
								{
									"Names"_= {"--include-hosts"}
									, "Default"_= true
									, "Description"_= "Include trusted and untrusted hosts in output"
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_ListNamespaces(_Params["IncludeTrustedHosts"].f_Boolean());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-namespace-add-trusted-host"}
							, "Category"_= Category
							, "Description"_= "Trust a host for the specified namespace.\n"
							, "Parameters"_=
							{
								"TrustHost"_= 
								{
									"Type"_= ""
									, "Description"_= "The host to trust."
								}
							}
							, "Options"_=
							{
								"TrustNamespace"_= 
								{
									"Names"_= {"--namespace"}
									, "Type"_= ""
									, "Description"_= "The namespace to trust the host for."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [this, _Params]
									{
										return f_CommandLine_TrustHostForNamespace(_Params["TrustNamespace"].f_String(), _Params["TrustHost"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-namespace-remove-trusted-host"}
							, "Category"_= Category
							, "Description"_= "Untrust a host for the specified namespace.\n"
							, "Parameters"_=
							{
								"TrustHost"_= 
								{
									"Type"_= ""
									, "Description"_= "The host to untrust."
								}
							}
							, "Options"_=
							{
								"TrustNamespace"_= 
								{
									"Names"_= {"--namespace"}
									, "Type"_= ""
									, "Description"_= "The namespace to untrust the host for."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Untrust host for namespace"
									, [this, _Params]
									{
										return f_CommandLine_UntrustHostForNamespace(_Params["TrustNamespace"].f_String(), _Params["TrustHost"].f_String());
									}
								)
							;
						}
					)
				;
				Category = "Host permissions";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-permission-list"}
							, "Category"_= Category
							, "Description"_= "List host permissions.\n"
							, "Output"_= "A new line separated list of permissions and the hosts that have those permissions."
							, "Options"_=
							{
								"IncludeHosts?"_= 
								{
									"Names"_= {"--include-hosts"}
									, "Default"_= true
									, "Description"_= "Include the hosts that have the permissions in the output"
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_ListHostPermissions(_Params["IncludeHosts"].f_Boolean());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-permission-add-host"}
							, "Category"_= Category
							, "Description"_= "Add a permission to a host.\n"
							, "Parameters"_=
							{
								"PermissionHost"_= 
								{
									"Type"_= ""
									, "Description"_= "The host to add a permission to."
								}
							}
							, "Options"_=
							{
								"Permission"_= 
								{
									"Names"_= {"--permission"}
									, "Type"_= ""
									, "Description"_= "The permission to add to the host."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [this, _Params]
									{
										return f_CommandLine_AddHostPermission(_Params["PermissionHost"].f_String(), _Params["Permission"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-permission-remove-host"}
							, "Category"_= Category
							, "Description"_= "Remove a permission to a host.\n"
							, "Parameters"_=
							{
								"PermissionHost"_= 
								{
									"Type"_= ""
									, "Description"_= "The host to remove a permission from."
								}
							}
							, "Options"_=
							{
								"Permission"_= 
								{
									"Names"_= {"--permission"}
									, "Type"_= ""
									, "Description"_= "The permission to remove from the host."
								}
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [this, _Params]
									{
										return f_CommandLine_RemoveHostPermission(_Params["PermissionHost"].f_String(), _Params["Permission"].f_String());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-host-id"}
							, "Description"_= "Get the host ID for this application.\n"
							, "Output"_= "The host ID of this application."
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_GetHostID();
						}
					)
				;
				Distributed.f_RegisterDirectCommand
					(
						{
							"Names"_= {"--trust-remove-all-trust"}
							, "Description"_= "Removes all certificates, clients, client connections and listen configs.\n"
							, "Options"_=
							{
								"Confirm?"_= 
								{
									"Names"_= {"--yes"}
									, "Default"_= false
									, "Description"_= "Confirm that you really want to remove all trust."
								}
							}
						}
						, [this](NEncoding::CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)
						{
							return f_CommandLine_RemoveAllTrust(_Parameters["Confirm"].f_Boolean());
						}
					)
				;
			}
		}
	}		
}
