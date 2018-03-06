// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Cryptography/RandomID>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib
{
	using namespace NStr;
	using namespace NFile;
	using namespace NPtr;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NProcess;

	namespace NConcurrency
	{
		ICCommandLine::ICCommandLine()
		{
			DMibPublishActorFunction(ICCommandLine::f_RunCommandLine);
		}

		ICCommandLineControl::ICCommandLineControl()
		{
			DMibPublishActorFunction(ICCommandLineControl::f_RegisterForStdIn);
			DMibPublishActorFunction(ICCommandLineControl::f_RegisterForStdInBinary);
			DMibPublishActorFunction(ICCommandLineControl::f_ReadBinary);
			DMibPublishActorFunction(ICCommandLineControl::f_ReadLine);
			DMibPublishActorFunction(ICCommandLineControl::f_ReadPrompt);
			DMibPublishActorFunction(ICCommandLineControl::f_AbortReads);
			DMibPublishActorFunction(ICCommandLineControl::f_StdOut);
			DMibPublishActorFunction(ICCommandLineControl::f_StdOutBinary);
			DMibPublishActorFunction(ICCommandLineControl::f_StdErr);
		}

		ICCommandLineControl::~ICCommandLineControl() = default;

		NConcurrency::TCContinuation<void> CCommandLineControl::f_StdOut(NStr::CStrSecure const &_Output) const
		{
			if (!m_ControlActor)
				return fg_Explicit();
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_StdOut, _Output);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::f_StdOutBinary(NContainer::CSecureByteVector const &_Output) const
		{
			if (!m_ControlActor)
				return fg_Explicit();
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_StdOutBinary, _Output);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::f_StdErr(NStr::CStrSecure const &_Output) const
		{
			if (!m_ControlActor)
				return fg_Explicit();
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_StdErr, _Output);
		}

		auto CCommandLineControl::f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnBinaryInput, NProcess::EStdInReaderFlag _Flags) const
			-> NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>>
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_RegisterForStdInBinary, fg_Move(_fOnBinaryInput), _Flags);
		}

		auto CCommandLineControl::f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const
			-> NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>>
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_RegisterForStdIn, fg_Move(_fOnInput), _Flags);
		}

		NConcurrency::TCContinuation<NContainer::CSecureByteVector> CCommandLineControl::f_ReadBinary() const
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_ReadBinary);
		}

		NConcurrency::TCContinuation<NStr::CStrSecure> CCommandLineControl::f_ReadLine() const
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_ReadLine);
		}

		NConcurrency::TCContinuation<NStr::CStrSecure> CCommandLineControl::f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) const
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_ReadPrompt, _Params);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::f_AbortReads() const
		{
			if (!m_ControlActor)
				return DMibErrorInstance("No control actor");
			return DMibCallActor(m_ControlActor, ICCommandLineControl::f_AbortReads);
		}

		uint32 CCommandLineControl::f_AddAsyncResult(CAsyncResult const &_Result) const
		{
			if (!_Result)
			{
				f_StdErr(fg_Format<CStrSecure>("{}\n", _Result.f_GetExceptionStr()));
				return 1;
			}
			return 0;
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator +=(NStr::CStrSecure const &_StdOut) const
		{
			return f_StdOut(_StdOut);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator %=(NStr::CStrSecure const &_StdErr) const
		{
			return f_StdErr(_StdErr);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator +=(NStr::CStr::CFormat const &_StdOut) const
		{
			return f_StdOut(CStr{_StdOut});
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator %=(NStr::CStr::CFormat const &_StdErr) const
		{
			return f_StdErr(CStr{_StdErr});
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator +=(NStr::CStrSecure::CFormat const &_StdOut) const
		{
			return f_StdOut(_StdOut);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator %=(NStr::CStrSecure::CFormat const &_StdErr) const
		{
			return f_StdErr(_StdErr);
		}

		NConcurrency::TCContinuation<void> CCommandLineControl::operator +=(NContainer::CSecureByteVector const &_StdOut) const
		{
			return f_StdOutBinary(_StdOut);
		}

		TCContinuation<void> CDistributedAppActor::fp_PreRunCommandLine
			(
				 CStr const &_Command
				 , NEncoding::CEJSON const &_Params
				 , NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			)
		{
			return fg_Explicit();
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_RunCommandLine
			(
				CCallingHostInfo const &_CallingHost
				, NStr::CStr const &_Command
				, NEncoding::CEJSON const &_Params
				, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
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
		
			TCContinuation<uint32> Continuation;
			fp_PreRunCommandLine(_Command, ValidatedParams, _pCommandLine) > Continuation / [=]()
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
						Command.m_fActorRunCommand(ValidatedParams, _pCommandLine) > Continuation;
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

		static bool fg_ValidateConnectionConcurrency(int32 _Concurrency, TCContinuation<uint32> &o_Continuation)
		{
			if (_Concurrency == -1 || (_Concurrency >= 1 && _Concurrency <= 128))
				return true;
			
			o_Continuation.f_SetException(DMibErrorInstance("Connection concurrency must be between 1 and 128 or -1"));
			return false;
		}
		
		auto CDistributedAppActor::f_CommandLine_AddConnection
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_Ticket
				 , bool _bIncludeFriendlyHostName
				 , int32 _ConnectionConcurrency
			)
			-> TCContinuation<uint32>
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
			
			TCContinuation<uint32> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, 30.0, _ConnectionConcurrency)
				> Continuation / [=](CHostInfo &&_HostInfo)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add connection to host '{}' from command line", _HostInfo.f_GetDesc());
					if (_bIncludeFriendlyHostName)
						*_pCommandLine += "{}\n"_f << _HostInfo.f_GetDesc();
					else
						*_pCommandLine += "{}\n"_f << _HostInfo.m_HostID;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_GenerateTrustTicket(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_ForListen)
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
				
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ForListen, nullptr) 
				> Continuation / [=](CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult &&_Ticket)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Generated trust ticket with address '{}' from command line", _Ticket.m_Ticket.m_ServerAddress.m_URL.f_Encode());
					*_pCommandLine += CStrSecure::CFormat("{}\n") << _Ticket.m_Ticket.f_ToStringTicket();
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_GetHostID(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID) 
				> Continuation / [=](CStr &&_HostID)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported host id '{}' to command line", _HostID);
					*_pCommandLine += "{}\n"_f << _HostID;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListTrustedHosts(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClients)
				> Continuation / [=](NContainer::TCMap<NStr::CStr, CHostInfo> &&_TrustedHosts)
				{
					CStr Result;
					for (auto &Host : _TrustedHosts)
					{
						if (_bIncludeFriendlyHostName)
							Result += "{}\n"_f << Host.f_GetDesc();
						else
							Result += "{}\n"_f <<  Host.m_HostID;
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported trusted hosts to command line");

					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveTrustedHost(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID)
				> Continuation / [Continuation, _HostID]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove trusted host '{}' from command line", _HostID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_GetConnetionStatus(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetConnectionState)
				> Continuation / [=](CDistributedActorTrustManager::CConnectionState &&_ConnectionState)
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
					auto fOutputLine = [&](CStr const &_URL, CStr const &_Connected, CStr const &_HostInfo, CStr const &_Concurrency, auto const &_ErrorTime, CStr const &_Error)
						{
							*_pCommandLine += "{sj*,a-}     {sj*,a-}     {sj9,a-}     {sj11,a-}     {sj23,a-}     {a-}\n"_f
								<< _URL
								<< LongestAddress
								<< _HostInfo
								<< LongestHostInfo
								<< _Connected
								<< _Concurrency
								<< _ErrorTime
								<< _Error
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
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		 
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListConnections(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections)
				> Continuation / [=]
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

						*_pCommandLine += "{}     {}     {}\n"_f << Address.m_URL.f_Encode() << HostInfo << ConnectionInfo.m_ConnectionConcurrency;
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported connections to command line");
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveConnection(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
		{
			TCContinuation<uint32> Continuation;
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, Address)
				> Continuation / [Continuation, _URL]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove connection '{}' from command line", _URL);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		auto CDistributedAppActor::f_CommandLine_AddAdditionalConnection
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_URL
				 , bool _bIncludeFriendlyHostName
				 , int32 _ConnectionConcurrency
			)
			-> TCContinuation<uint32>
		{
			TCContinuation<uint32> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, Address, _ConnectionConcurrency)
				> Continuation / [=](CHostInfo &&_HostInfo)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add additional connection to host '{}' from command line", _HostInfo.f_GetDesc());
					if (_bIncludeFriendlyHostName)
						*_pCommandLine += "{}\n"_f << _HostInfo.f_GetDesc();
					else
						*_pCommandLine += "{}\n"_f << _HostInfo.m_HostID;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_SetConnectionConcurrency
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_URL
				 , int32 _ConnectionConcurrency
			)
		{
			TCContinuation<uint32> Continuation;
			
			if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Continuation))
				return Continuation;
			
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _URL;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, Address, _ConnectionConcurrency)
				> Continuation / [Continuation, Address, _ConnectionConcurrency]()
				{
					(void)_ConnectionConcurrency;
					
					DMibLogWithCategory
						(
							Mib/Concurrency/App
							, Info
							, "Change connection concurrency to {} for address '{}' from command line"
							, _ConnectionConcurrency
							, Address.m_URL.f_Encode()
						)
					;
					Continuation.f_SetResult(0);
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
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_AddListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
		{
			TCContinuation<uint32> Continuation;
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
							Continuation.f_SetResult(0);
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
		{
			TCContinuation<uint32> Continuation;
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
							Continuation.f_SetResult(0);
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_SetPrimaryListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
		{
			TCContinuation<uint32> Continuation;
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
							Continuation.f_SetResult(0);
						}
					;
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListListen(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens)
				> Continuation / [=](NContainer::TCSet<CDistributedActorTrustManager_Address> &&_Listens)
				{
					CStr Result;
					for (auto &Listen : _Listens)
						Result += "{}\n"_f << Listen.m_URL.f_Encode();
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListNamespaces(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTrustedHosts)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeTrustedHosts)
				> Continuation / [=](NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> &&_Namespaces)
				{
					CStr Result;
					for (auto &Permissions : _Namespaces)
					{
						auto &Namespace = _Namespaces.fs_GetKey(Permissions);
						Result += "{}\n"_f << Namespace;
						if (_bIncludeTrustedHosts)
						{
							Result += "    Trusted\n";
							for (auto &Host : Permissions.m_AllowedHosts)
								Result += "        {}\n"_f << Host.f_GetDesc();
							Result += "    Untrusted\n";
							for (auto &Host : Permissions.m_DisallowedHosts)
								Result += "        {}\n"_f << Host.f_GetDesc();
						}
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported namespaces to command line");
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_TrustHostForNamespace
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Namespace
				, NStr::CStr const &_Host
			)
		{
			TCContinuation<uint32> Continuation;
			NContainer::TCSet<NStr::CStr> Hosts;
			Hosts[_Host];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
				> Continuation / [Continuation, _Namespace, _Host]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Trusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_UntrustHostForNamespace
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, NStr::CStr const &_Namespace
				, NStr::CStr const &_Host
			)
		{
			TCContinuation<uint32> Continuation;
			NContainer::TCSet<NStr::CStr> Hosts;
			Hosts[_Host];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
				> Continuation / [Continuation, _Namespace, _Host]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Untrusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}


		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListHostPermissions(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeHosts)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumHostPermissions, _bIncludeHosts)
				> Continuation / [=](NContainer::TCMap<NStr::CStr, NContainer::TCMap<NStr::CStr, CHostInfo>> &&_Permissions)
				{
					CStr Result;
					for (auto &Permission : _Permissions)
					{
						auto &PermissionName = _Permissions.fs_GetKey(Permission);
						Result += "{}\n"_f << PermissionName;
						if (_bIncludeHosts)
						{
							for (auto &Host : Permission)
								Result += "    {}\n"_f << Host.f_GetDesc();
						}
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported permissions to command line {}", Result);
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_AddHostPermission
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_HostID, NStr::CStr const &_Permission
			)
		{
			TCContinuation<uint32> Continuation;
			NContainer::TCSet<NStr::CStr> Permissions;
			Permissions[_Permission];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddHostPermissions, _HostID, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
				> Continuation / [Continuation, _HostID, _Permission]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add permission '{}' to host '{}' from command line", _Permission, _HostID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}
		
		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveHostPermission
			(
				 NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				 , NStr::CStr const &_HostID
				 , NStr::CStr const &_Permission
			)
		{
			TCContinuation<uint32> Continuation;
			NContainer::TCSet<NStr::CStr> Permissions;
			Permissions[_Permission];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveHostPermissions, _HostID, Permissions, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
				> Continuation / [Continuation, _HostID, _Permission]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove permission '{}' from host '{}' from command line", _Permission, _HostID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		uint32 CDistributedAppActor::f_CommandLine_RemoveAllTrust(bool _bConfirm)
		{
			CStr TrustDatabaseLocation = fg_Format("{}/TrustDatabase.{}", mp_Settings.m_RootDirectory, mp_Settings.m_AppName);
			CStr TrustDatabaseCommandLineLocation = fg_Format("{}/CommandLineTrustDatabase.{}", mp_Settings.m_RootDirectory, mp_Settings.m_AppName);
			
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

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_AddUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
		{
			TCContinuation<uint32> Continuation;
			CStr const &_UserName = _Params["UserName"].f_String();
			CStr const &UserID = NCryptography::fg_RandomID();
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddUser, UserID, _UserName)
				> Continuation / [Continuation, UserID, _UserName]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Add user ID '{}' ({}) to host from command line", UserID, _UserName);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveUser, _UserID)
				> Continuation / [Continuation, _UserID]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove user '{}' from command line", _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ListUsers(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUsers, false)
				> Continuation / [=](NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> &&_Users)
				{
					CStr Result;
					for (auto &UserInfo : _Users)
					{
						auto &UserID = _Users.fs_GetKey(UserInfo);
						Result += "{}\n"_f << UserID;
						Result += "    {}\n"_f << UserInfo.m_UserName;
					}
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported users to command line {}", Result);
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_SetUserInfo(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
		{
			TCContinuation<uint32> Continuation;
			CStr const &UserID = _Params["UserID"].f_String();
			TCOptional<CStr> UserName;
			TCMap<CStr, CEJSON> AddMetadata;
			TCSet<CStr> RemoveMetadata;

			if (auto pValue = _Params.f_GetMember("UserName"))
				UserName = pValue->f_String();

			if (auto pValue = _Params.f_GetMember("Metadata"))
			{
				auto Object = pValue->f_Object();
				for (auto iMetadata = Object.f_OrderedIterator(); iMetadata; ++iMetadata)
					AddMetadata[iMetadata->f_Name()] = iMetadata->f_Value();
			}
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo,  UserID, UserName, RemoveMetadata, AddMetadata)
				> Continuation / [Continuation, UserID]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Set metadata for user '{}' from command line", UserID);
					Continuation.f_SetResult(0);
				}
			;

			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RemoveMetadata(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, CStr const &_Key)
		{
			TCContinuation<uint32> Continuation;
			TCOptional<CStr> UserName;
			TCMap<CStr, CEJSON> AddMetadata;
			TCSet<CStr> RemoveMetadata;
			RemoveMetadata[_Key];
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo, _UserID, UserName, AddMetadata, RemoveMetadata)
				> Continuation / [Continuation, _UserID, _Key]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove metadata '{}' from user '{}' from command line", _Key, _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ExportUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, bool _bIncludePrivate)
		{
			TCContinuation<uint32> Continuation;
			fg_ExportUser(mp_State.m_TrustManager, _UserID, _bIncludePrivate) > Continuation / [Continuation, _UserID, _pCommandLine](CStr const &_UserData)
				{
					*_pCommandLine += _UserData;
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Exported user ID '{}' from command line", _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_ImportUser(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserData)
		{
			TCContinuation<uint32> Continuation;
			fg_ImportUser(mp_State.m_TrustManager, _UserData) > Continuation / [Continuation](CStr const &_UserID)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Import user ID '{}' from command line", _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_RegisterAuthenticationFactor
			(
				TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_UserID
				, CStr const &_Factor
			)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RegisterAuthenticationFactor, _pCommandLine, _UserID, _Factor)
				> Continuation / [Continuation, _UserID, _Factor](CStr const &_FactorID)
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Added authentification factor '{}' of type '{}' to user '{}' from command line", _FactorID, _Factor, _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_UnregisterAuthenticationFactor
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_UserID
				, CStr const &_Factor
			)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveAuthenticationFactor, _UserID, _Factor)
				> Continuation / [Continuation, _UserID, _Factor]()
				{
					DMibLogWithCategory(Mib/Concurrency/App, Info, "Removed authentification factor '{}' from user '{}' from command line", _Factor, _UserID);
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_EnumUserAuthenticationFactors(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _UserID)
				> Continuation / [Continuation, _UserID, _pCommandLine](TCMap<CStr, CAuthenticationData> &&_Factors)
				{
					CStr Result;
					for (auto &Factor : _Factors)
					{
						Result += "{}\n\t{}\n"_f << _Factors.fs_GetKey(Factor) << Factor.m_Name;
					}
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::f_CommandLine_EnumAuthenticationFactors(NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
		{
			TCContinuation<uint32> Continuation;
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationFactors)
				> Continuation / [Continuation, _pCommandLine](TCSet<CStr> &&_Factors)
				{
					CStr Result;
					for (auto &Factor : _Factors)
					{
						Result += "{}\n"_f << Factor;
					}
					*_pCommandLine += Result;
					Continuation.f_SetResult(0);
				}
			;
			return Continuation;
		}

		TCContinuation<uint32> CDistributedAppActor::fp_RunCommandLineAndLogError
			(
				CStr const &_Description
				, NFunction::TCFunction<TCContinuation<uint32> ()> &&_fCommand
			)
		{
			TCContinuation<uint32> Continuation;
			fg_Dispatch
				(
					[fCommand = fg_Move(_fCommand)]
					{
						return fCommand();
					}
				)
				> [Continuation, _Description](TCAsyncResult<uint32> &&_Other)
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListConnections(_pCommandLine, _Params["IncludeFriendlyName"].f_Boolean());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_GetConnetionStatus(_pCommandLine);
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add trust connection"
									, [=]
									{
										return f_CommandLine_AddConnection
											(
												_pCommandLine
												, _Params["Ticket"].f_String()
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Set connection concurrency"
									, [=]
									{
										return f_CommandLine_SetConnectionConcurrency
											(
												_pCommandLine
												, _Params["Address"].f_String()
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add additional trust connection"
									, [=]
									{
										return f_CommandLine_AddAdditionalConnection
											(
												_pCommandLine
												, _Params["ConnectionURL"].f_String()
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove trust connection"
									, [=]
									{
										return f_CommandLine_RemoveConnection(_pCommandLine, _Params["ConnectionURL"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListTrustedHosts(_pCommandLine, _Params["IncludeFriendlyName"].f_Boolean());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove trusted host"
									, [=]
									{
										return f_CommandLine_RemoveTrustedHost(_pCommandLine, _Params["HostID"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Generate trust ticket"
									, [=]
									{
										return f_CommandLine_GenerateTrustTicket(_pCommandLine, _Params["ForListenURL"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListListen(_pCommandLine);
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Add trust listen address"
									, [=]
									{
										return f_CommandLine_AddListen(_pCommandLine, _Params["ListenURL"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Remove listen address"
									, [=]
									{
										return f_CommandLine_RemoveListen(_pCommandLine, _Params["ListenURL"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Set primary listen address"
									, [=]
									{
										return f_CommandLine_SetPrimaryListen(_pCommandLine, _Params["ListenURL"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListNamespaces(_pCommandLine, _Params["IncludeTrustedHosts"].f_Boolean());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [=]
									{
										return f_CommandLine_TrustHostForNamespace(_pCommandLine, _Params["TrustNamespace"].f_String(), _Params["TrustHost"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Untrust host for namespace"
									, [=]
									{
										return f_CommandLine_UntrustHostForNamespace(_pCommandLine, _Params["TrustNamespace"].f_String(), _Params["TrustHost"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListHostPermissions(_pCommandLine, _Params["IncludeHosts"].f_Boolean());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [=]
									{
										return f_CommandLine_AddHostPermission(_pCommandLine, _Params["PermissionHost"].f_String(), _Params["Permission"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return fp_RunCommandLineAndLogError
								(
									"Trusted host for namespace"
									, [=]
									{
										return f_CommandLine_RemoveHostPermission(_pCommandLine, _Params["PermissionHost"].f_String(), _Params["Permission"].f_String());
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
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_GetHostID(_pCommandLine);
						}
					)
				;
				Category = "User management (PRERELEASE)";
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-add"}
							, "Category"_= Category
							, "Description"_= "Add a user to the database.\n"
							, "Output"_= "None."
							, "Parameters"_=
							{
								"UserName"_=
								{
									"Type"_= ""
									, "Description"_= "The name of the user being added to the database"
								}
							}
					}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_AddUser(_pCommandLine, _Params);
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-remove"}
							, "Category"_= Category
							, "Description"_= "Remove a user from the database.\n"
							, "Output"_= "None."
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID to remove from the database"
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_RemoveUser(_pCommandLine, _Params["UserID"].f_String());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-list"}
							, "Category"_= Category
							, "Description"_= "List the users in the database.\n"
							, "Output"_= "A newline separated list of the users."
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ListUsers(_pCommandLine);
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-set-userinfo"}
							, "Category"_= Category
							, "Description"_= "Set or add metadata to a user.\n"
							"Change user name or add new key, value pairs to the user's metadata or replace a value if the key already exists."
							, "Options"_=
							{
								"Metadata?"_=
								{
									"Names"_= {"--metadata"}
									, "Type"_= EJSONType_Object
									, "Description"_= "The metadata to set.\n"
									"The metadata is specified as a JSON object '{\"Key\" : \"Value\"}'"
								}
								, "UserName?"_=
								{
									"Names"_= {"--username"}
									, "Type"_= ""
									, "Description"_= "The new username.\n"
								}
							}
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID to change the information for."
								}
							}

						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_SetUserInfo(_pCommandLine, _Params);
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-remove-metadata"}
							, "Category"_= Category
							, "Description"_= "Remove the metadata matching key from the user."
							, "Options"_=
							{

								"Key"_=
								{
									"Names"_= {"--key"}
									, "Type"_= ""
									, "Description"_= "Key of the metadata to remove.\n"
								}
							}
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID to remove metadata from"
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_RemoveMetadata(_pCommandLine, _Params["UserID"].f_String(), _Params["Key"].f_String());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-export"}
							, "Category"_= Category
							, "Description"_= "Export a user from the database.\n"
							, "Output"_= "An encoded text representation of the user data."
							, "Options"_=
							{

								"IncludePrivate?"_=
								{
									"Names"_= {"--include-private-data"}
									, "Default"_= true
									, "Description"_= "In addition to data needed to verify a users identity, include private data needed to authenticate as that user.\n"
								}
							}
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID of the user to export"
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							bool bIncludePrivate = false;
							if (auto *pValue = _Params.f_GetMember("IncludePrivate"))
							{
								bIncludePrivate = pValue->f_Boolean();
							}

							return f_CommandLine_ExportUser(_pCommandLine, _Params["UserID"].f_String(), bIncludePrivate);
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-import"}
							, "Category"_= Category
							, "Description"_= "Import a user into the database.\n"
							, "Output"_= "None."
							, "Parameters"_=
							{
								"UserData"_=
								{
									"Type"_= ""
									, "Description"_= "The encoded text representation of the user data"
								}
							}
					}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_ImportUser(_pCommandLine, _Params["UserData"].f_String());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-add-authentication-factor"}
							, "Category"_= Category
							, "Description"_= "Add an authentication factor to the user."
							, "Options"_=
							{
								"Factor"_=
								{
									"Names"_= {"--authentication-factor"}
									, "Type"_= ""
									, "Description"_= "The authentication factor to add.\n"
								}
							}
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID to add an authentication factor to."
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_RegisterAuthenticationFactor(_pCommandLine, _Params["UserID"].f_String(), _Params["Factor"].f_String());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-remove-authentication-factor"}
							, "Category"_= Category
							, "Description"_= "Remove an authentication factor to the user."
							, "Options"_=
							{
								"Factor"_=
								{
									"Names"_= {"--authentication-factor"}
									, "Type"_= ""
									, "Description"_= "The authentication factor to remove.\n"
								}
							}
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "The user ID to remove an authentication factor from."
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_UnregisterAuthenticationFactor(_pCommandLine, _Params["UserID"].f_String(), _Params["Factor"].f_String());
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-list-available-authentication-factor-types"}
							, "Category"_= Category
							, "Description"_= "List available authentication factors."
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_EnumAuthenticationFactors(_pCommandLine);
						}
					)
				;
				Distributed.f_RegisterCommand
					(
						{
							"Names"_= {"--trust-user-list-authentication-factors"}
							, "Category"_= Category
							, "Description"_= "List the authentication factors registered to a user."
							, "Parameters"_=
							{
								"UserID"_=
								{
									"Type"_= ""
									, "Description"_= "List the authentication factors registered for the user."
								}
							}
						}
						, [this](CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						{
							return f_CommandLine_EnumUserAuthenticationFactors(_pCommandLine, _Params["UserID"].f_String());
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
