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
		CDistributedAppCommandLineResults::COutput::COutput() = default;
		
		CDistributedAppCommandLineResults::COutput::COutput(NStr::CStr const &_Output, EOutputType _OutputType)
			: m_OutputType(_OutputType)
			, m_Output(_Output)
		{
		}

		CDistributedAppCommandLineResults::CDistributedAppCommandLineResults() = default;
		
		CDistributedAppCommandLineResults::CDistributedAppCommandLineResults(NStr::CStr const &_Output, EOutputType _OutputType)
		{
			m_Output.f_Insert(fg_Construct(_Output, _OutputType));
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
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_RunCommandLine
			(
				NStr::CStr const &_FromHostID
				, NStr::CStr const &_Command
				, NEncoding::CEJSON const &_Params
			)
		{
			if (!fp_HasCommandLineAccess(_FromHostID))
				return DMibErrorInstance("Access denied");
			
			auto &SpecInternal = *(mp_pCommandLineSpec->mp_pInternal);
			
			auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
			
			if (!pCommand)
				return DMibErrorInstance("Non-existant command line command");

			auto &Command = **pCommand;
			if (!Command.m_fActorRunCommand)
				return DMibErrorInstance("Non-actor cammand");

			try
			{
				return Command.m_fActorRunCommand(SpecInternal.f_ValidateParams(Command, _Params));
			}
			catch (NException::CException const &_Exception)
			{
				return _Exception;
			}
		}
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_AddConnection(NStr::CStr const &_Ticket, bool _bIncludeFriendlyHostName)
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
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, 30.0) 
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
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ForListen) 
				> Continuation / [Continuation](CDistributedActorTrustManager::CTrustTicket &&_Ticket)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Generated trust ticket with address '{}' from command line", _Ticket.m_ServerAddress.m_URL.f_Encode());
					Continuation.f_SetResult(fg_Format("{}\n", _Ticket.f_ToStringTicket()));
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
							LongestAddress = fg_Max(State.f_GetAddress().m_URL.f_Encode().f_GetLen(), LongestAddress);
							LongestHostInfo = fg_Max(State.m_HostInfo.f_GetDesc().f_GetLen(), LongestHostInfo);
							AddressState[State.f_GetAddress()] = State;
						}
					}
					CStr Result;
					auto fOutputLine = [&](CStr const &_URL, CStr const &_Connected, CStr const &_HostInfo, auto const &_ErrorTime, CStr const &_Error)
						{
							Result += fg_Format("{sj*,a-}     {sj9,a-}     {sj*,a-}     {sj23,a-}     {a-}\n", _URL, LongestAddress, _Connected, _HostInfo, LongestHostInfo, _ErrorTime, _Error);
						}
					;
					fOutputLine("URL", "Connected", "Host", "Last error time", "Last error");
					for (auto &State : AddressState)
					{
						auto &Address = AddressState.fs_GetKey(State); 
						fOutputLine
							(
								Address.m_URL.f_Encode()
								, State.m_bConnected ? "Yes" : "NO"
								, State.m_HostInfo.f_GetDesc()
								, State.m_ErrorTime.f_IsValid() ? fg_Format("{}", State.m_ErrorTime) : "Never"
								, !State.m_Error.f_IsEmpty() ? State.m_Error : "None"
							)
						;
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
				> Continuation / [Continuation, _bIncludeFriendlyHostName](NContainer::TCMap<CDistributedActorTrustManager_Address, CHostInfo> &&_ClientConnections)
				{
					CStr Result;
					for (auto &Host : _ClientConnections)
					{
						auto &Address = _ClientConnections.fs_GetKey(Host); 
						if (_bIncludeFriendlyHostName)
							Result += fg_Format("{}     {}\n", Address.m_URL.f_Encode(), Host.f_GetDesc());
						else
							Result += fg_Format("{}     {}\n", Address.m_URL.f_Encode(), Host.m_HostID);
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
		
		TCContinuation<CDistributedAppCommandLineResults> CDistributedAppActor::f_CommandLine_AddAdditionalConnection(NStr::CStr const &_URL, bool _bIncludeFriendlyHostName)
		{
			TCContinuation<CDistributedAppCommandLineResults> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, Address) 
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
					ListenArray.f_InsertFirst({"Address"_= _Address});
					return true;
				}
				else
				{
					ListenArray.f_Insert({"Address"_= _Address});
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
						"ConcurrentLoging?"_=
						{
							"Names"_= {"--log-on-thread"}
							, "Default"_= true
							, "Description"_= "Do logging on a separate thread to minimally affect application performance and behavior"
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
				auto Distributed = o_CommandLine.f_AddSection("Distributed Computing", "These commands are used to manage connectability and trust between different hosts.");
				auto IncludeFriendlyNameOption = "IncludeFriendlyName?"_= 
					{
						"Names"_= {"--include-friendly-name"}
						, "Default"_= true
						, "Description"_= "Include friendly host name in output"
					}
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-list-connections"}
							, "Description"_= "Lists the outgoing addresses that the application tries to connect to.\n"
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
							, "Description"_= "Lists the status for all outgoing connections.\n"
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
							"Names"_= {"--trust-add-connection"}
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
								IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add trust connection"
									, [this, _Params]
									{
										return f_CommandLine_AddConnection(_Params["Ticket"].f_String(), _Params["IncludeFriendlyName"].f_Boolean());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-add-additional-connection"}
							, "Description"_= "Adds a connection to an already known host.\n"
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
								IncludeFriendlyNameOption
							}
						}
						, [this](CEJSON const &_Params)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add additional trust connection"
									, [this, _Params]
									{
										return f_CommandLine_AddAdditionalConnection(_Params["ConnectionURL"].f_String(), _Params["IncludeFriendlyName"].f_Boolean());
									}
								)
							;
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-remove-connection"}
							, "Description"_= "Removes an outgoing connection\n"
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
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-generate-ticket"}
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
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-list-trusted"}
							, "Description"_= "Lists the hosts that are trusted by this application.\n"
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
							"Names"_= {"--trust-remove-trusted"}
							, "Description"_= "Removes the trust for a host id.\n"
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
							"Names"_= {"--trust-list-listen"}
							, "Description"_= "Lists addresses that this application listens for incoming connections on.\n"
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
							"Names"_= {"--trust-add-listen"}
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
							"Names"_= {"--trust-set-primary-listen"}
							, "Description"_= "Sets the primary listen URL.\n"
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
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-remove-listen"}
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
							"Names"_= {"--trust-host-id"}
							, "Description"_= "Gets the host ID for this application.\n"
							, "Output"_= "The host ID of this application."
						}
						, [this](CEJSON const &_Params)
						{
							return f_CommandLine_GetHostID();
						}
					)
				;
			}
		}
	}		
}
