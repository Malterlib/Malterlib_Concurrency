// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/DistributedActorTrustManagerAuthenticationActor>
#include <Mib/Process/StdIn>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NProcess;
	using namespace NCommandLine;

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

	TCFuture<void> CCommandLineControl::f_StdOut(NStr::CStrSecure const &_Output) const
	{
		if (!m_ControlActor)
			return fg_Explicit();
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_Output);
	}

	TCFuture<void> CCommandLineControl::f_StdOutBinary(NContainer::CSecureByteVector const &_Output) const
	{
		if (!m_ControlActor)
			return fg_Explicit();
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(_Output);
	}

	TCFuture<void> CCommandLineControl::f_StdErr(NStr::CStrSecure const &_Output) const
	{
		if (!m_ControlActor)
			return fg_Explicit();
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_Output);
	}

	CTableRenderHelper CCommandLineControl::f_TableRenderer() const
	{
		return CTableRenderHelper
			(
				[this](NStr::CStr const &_Output)
			 	{
					*this += _Output;
				}
			 	, CTableRenderHelper::EOption_Rounded
			 	, m_AnsiFlags
		 	)
		;
	}

	NCommandLine::CAnsiEncoding CCommandLineControl::f_AnsiEncoding() const
	{
		return NCommandLine::CAnsiEncoding(m_AnsiFlags);
	}

	auto CCommandLineControl::f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnBinaryInput, NProcess::EStdInReaderFlag _Flags) const
		-> TCFuture<TCActorSubscriptionWithID<>>
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdInBinary)(fg_Move(_fOnBinaryInput), _Flags);
	}

	auto CCommandLineControl::f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const
		-> TCFuture<TCActorSubscriptionWithID<>>
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdIn)(fg_Move(_fOnInput), _Flags);
	}

	TCFuture<NContainer::CSecureByteVector> CCommandLineControl::f_ReadBinary() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadBinary)();
	}

	TCFuture<NStr::CStrSecure> CCommandLineControl::f_ReadLine() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadLine)();
	}

	TCFuture<NStr::CStrSecure> CCommandLineControl::f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadPrompt)(_Params);
	}

	TCFuture<void> CCommandLineControl::f_AbortReads() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_AbortReads)();
	}

	uint32 CCommandLineControl::f_AddAsyncResult(CAsyncResult const &_Result) const
	{
		if (!_Result)
		{
			(void)f_StdErr(fg_Format<CStrSecure>("{}\n", _Result.f_GetExceptionStr()));
			return 1;
		}
		return 0;
	}

	void CCommandLineControl::operator +=(NStr::CStrSecure const &_StdOut) const
	{
		(void)f_StdOut(_StdOut);
	}

	void CCommandLineControl::operator %=(NStr::CStrSecure const &_StdErr) const
	{
		(void)f_StdErr(_StdErr);
	}

	void CCommandLineControl::operator +=(NStr::CStr::CFormat const &_StdOut) const
	{
		(void)f_StdOut(CStr{_StdOut});
	}

	void CCommandLineControl::operator %=(NStr::CStr::CFormat const &_StdErr) const
	{
		(void)f_StdErr(CStr{_StdErr});
	}

	void CCommandLineControl::operator +=(NStr::CStrSecure::CFormat const &_StdOut) const
	{
		(void)f_StdOut(_StdOut);
	}

	void CCommandLineControl::operator %=(NStr::CStrSecure::CFormat const &_StdErr) const
	{
		(void)f_StdErr(_StdErr);
	}

	void CCommandLineControl::operator +=(NContainer::CSecureByteVector const &_StdOut) const
	{
		(void)f_StdOutBinary(_StdOut);
	}

	TCFuture<void> CDistributedAppActor::fp_PreRunCommandLine
		(
			 CStr const &_Command
			 , NEncoding::CEJSON const &_Params
			 , NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		return fg_Explicit();
	}

	TCFuture<uint32> CDistributedAppActor::f_RunCommandLine
		(
			CCallingHostInfo const &_CallingHost
			, NStr::CStr const &_Command
			, NEncoding::CEJSON const &_Params
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
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

		TCPromise<void> ConnectionWaitPromise;
		if ((*pCommand)->m_Flags & CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes)
			mp_State.m_TrustManager(&CDistributedActorTrustManager::f_WaitForInitialConnection) > ConnectionWaitPromise;
		else
			ConnectionWaitPromise.f_SetResult();

		TCPromise<uint32> Promise;

		ConnectionWaitPromise > Promise / [=]
			{
				CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};

				int64 AuthenticationLifetime = ValidatedParams.f_GetMemberValue("AuthenticationLifetime", CPermissionRequirements::mc_OverrideLifetimeNotSet).f_Integer();
				if (AuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet && _Command == "--trust-user-authenticate-pattern")
					AuthenticationLifetime = CPermissionRequirements::mc_DefaultMaximumLifetime;

				CStr UserID = ValidatedParams.f_GetMemberValue("AuthenticationUser", "").f_String();

				self(&CDistributedAppActor::fp_SetupAuthentication, _pCommandLine, AuthenticationLifetime, UserID) > Promise / [=](CActorSubscription &&_AuthenticationSubscription)
					{
						CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};
						self(&CDistributedAppActor::fp_PreRunCommandLine, _Command, ValidatedParams, _pCommandLine) > Promise / [=, AuthenticationSubscription = fg_Move(_AuthenticationSubscription)]() mutable
							{
								auto &SpecInternal = *(mp_pCommandLineSpec->mp_pInternal);

								auto *pCommand = SpecInternal.m_CommandByName.f_FindEqual(_Command);
								if (!pCommand)
								{
									Promise.f_SetException(DMibErrorInstance("Non-existant command line command"));
									return;
								}

								auto &Command = **pCommand;
								if (!Command.m_fActorRunCommand)
								{
									Promise.f_SetException(DMibErrorInstance("Non-actor cammand"));
									return;
								}

								try
								{
									CCallingHostInfoScope CallingHostInfoScope{fg_TempCopy(_CallingHost)};
									Command.m_fActorRunCommand(ValidatedParams, _pCommandLine)
										> Promise / [Promise, AuthenticationSubscription = fg_Move(AuthenticationSubscription)](uint32 &&_Result)
										{
											Promise.f_SetResult(_Result);
										}
									;
								}
								catch (NException::CException const &_Exception)
								{
									Promise.f_SetException(_Exception);
									return;
								}
							}
						;
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	static bool fg_ValidateConnectionConcurrency(int32 _Concurrency, TCPromise<uint32> &o_Promise)
	{
		if (_Concurrency == -1 || (_Concurrency >= 1 && _Concurrency <= 128))
			return true;

		o_Promise.f_SetException(DMibErrorInstance("Connection concurrency must be between 1 and 128 or -1"));
		return false;
	}

	auto CDistributedAppActor::f_CommandLine_AddConnection
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NStr::CStr const &_Ticket
			, bool _bIncludeFriendlyHostName
			, int32 _ConnectionConcurrency
			, NContainer::TCSet<NStr::CStr> &&_TrustedNamespaces
		)
		-> TCFuture<uint32>
	{
		for (auto &Namespace : _TrustedNamespaces)
		{
			if (!CActorDistributionManager::fs_IsValidNamespaceName(Namespace))
				return DMibErrorInstance("'{}' is not a valid namespace name"_f << Namespace);
		}

		TCPromise<uint32> Promise;
		if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Promise))
			return Promise.f_MoveFuture();

		TCFuture<CStrSecure> TicketFuture;
		if (_Ticket.f_IsEmpty())
			TicketFuture = _pCommandLine->f_ReadPrompt({"Please paste connection ticket: ", false});
		else
			TicketFuture = fg_Explicit(_Ticket);

		TicketFuture > Promise / [=](CStrSecure &&_TicketStr) mutable
			{
				CDistributedActorTrustManager::CTrustTicket Ticket;
				try
				{
					NException::CDisableExceptionTraceScope DisableExceptionTrace;
					Ticket = CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(_TicketStr);
				}
				catch (NException::CException const &_Exception)
				{
					return Promise.f_SetException(DMibErrorInstance(fg_Format("Faild to decode ticket: {}", _Exception.f_GetErrorStr())));
				}

				mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, 30.0, _ConnectionConcurrency)
					> Promise / [=](CHostInfo &&_HostInfo)
					{
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Add connection to host '{}' from command line", _HostInfo.f_GetDesc());

						TCPromise<void> TrustNamespaceContination;
						if (!_TrustedNamespaces.f_IsEmpty())
						{
							TCActorResultMap<CStr, void> TrustResults;

							NContainer::TCSet<NStr::CStr> Hosts;
							Hosts[_HostInfo.m_HostID];

							CStr HostID = _HostInfo.m_HostID;

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

							TrustResults.f_GetResults() > [=](TCAsyncResult<TCMap<CStr, TCAsyncResult<void>>> &&_Results)
								{
									if (!_Results)
										*_pCommandLine %= "Failed to trust namespaces: {}\n"_f << _Results.f_GetExceptionStr();
									else
									{
										TCSet<CStr> SuccessfulNamespaces;
										for (auto &NamespaceResult : *_Results)
										{
											CStr const &NamespaceName = _Results->fs_GetKey(NamespaceResult);
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

									TrustNamespaceContination.f_SetResult();
								}
							;
						}
						else
							TrustNamespaceContination.f_SetResult();

						TrustNamespaceContination > TrustNamespaceContination / [=]
							{
								if (_bIncludeFriendlyHostName)
									*_pCommandLine += "{}\n"_f << _HostInfo.f_GetDesc();
								else
									*_pCommandLine += "{}\n"_f << _HostInfo.m_HostID;
								Promise.f_SetResult(0);
							}
						;
					}
				;
			}
		;

		return Promise.f_MoveFuture();
	}

	namespace
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

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GenerateTrustTicket
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_ForListen
			, TCSet<CStr> const &_Permissions
			, NStr::CStr const &_UserID
			, NEncoding::CEJSON const &_AuthenticationFactors
		)
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
		auto Requirements = fg_ParsePermissionRequirements(_AuthenticationFactors);

		TCPromise<uint32> Promise;
		mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, ForListen
				, nullptr
				, bHasPermissions ? g_ActorFunctor / [this, _Permissions, pCleanup = fg_Move(pCleanup), _UserID, Requirements]
				(
					NStr::CStr const &_HostID
					, CCallingHostInfo const &_HostInfo
				) mutable -> TCFuture<void>
				{
					if (_Permissions.f_IsEmpty())
						return fg_Explicit();

					NContainer::TCMap<NStr::CStr, CPermissionRequirements> Permissions;
					for (auto const &Permission : _Permissions)
						Permissions[Permission] = Requirements;

					TCPromise<void> Promise;
					mp_State.m_TrustManager
						(
							&CDistributedActorTrustManager::f_AddPermissions
							, CPermissionIdentifiers{_HostID, _UserID}
							, Permissions
							, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
						)
						> Promise / [Promise, _HostID, _Permissions, pCleanup = fg_Move(pCleanup), _UserID]()
						{
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Add permissions {vs} to host '{}' for user '{}' as part of ticket usage", _Permissions, _HostID, _UserID);
							Promise.f_SetResult();
						}
					;
					return Promise.f_MoveFuture();
				}
				: nullptr
			)
			> Promise / [=](CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult &&_Ticket)
			{
				if (bHasPermissions)
					mp_TicketPermissionSubscriptions[TicketPermissionRequestID] = fg_Move(_Ticket.m_NotificationsSubscription);

				DMibLogWithCategory(Mib/Concurrency/App, Info, "Generated trust ticket with address '{}' from command line", _Ticket.m_Ticket.m_ServerAddress.m_URL.f_Encode());
				*_pCommandLine += CStrSecure::CFormat("{}\n") << _Ticket.m_Ticket.f_ToStringTicket();
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetHostID(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetHostID)
			> Promise / [=](CStr &&_HostID)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported host id '{}' to command line", _HostID);
				*_pCommandLine += "{}\n"_f << _HostID;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListTrustedHosts(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClients)
			> Promise / [=](NContainer::TCMap<NStr::CStr, CHostInfo> &&_TrustedHosts)
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
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveTrustedHost(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_HostID)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClient, _HostID)
			> Promise / [Promise, _HostID]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove trusted host '{}' from command line", _HostID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetConnetionStatus(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetConnectionState)
			> Promise / [=](CDistributedActorTrustManager::CConnectionState &&_ConnectionState)
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
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListConnections(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeFriendlyHostName)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumClientConnections)
			> Promise / [=]
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
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveConnection(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
	{
		TCPromise<uint32> Promise;
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveClientConnection, Address, false)
			> Promise / [Promise, _URL]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove connection '{}' from command line", _URL);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	auto CDistributedAppActor::f_CommandLine_AddAdditionalConnection
		(
			 NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 , NStr::CStr const &_URL
			 , bool _bIncludeFriendlyHostName
			 , int32 _ConnectionConcurrency
		)
		-> TCFuture<uint32>
	{
		TCPromise<uint32> Promise;

		if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Promise))
			return Promise.f_MoveFuture();

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddAdditionalClientConnection, Address, _ConnectionConcurrency)
			> Promise / [=](CHostInfo &&_HostInfo)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Add additional connection to host '{}' from command line", _HostInfo.f_GetDesc());
				if (_bIncludeFriendlyHostName)
					*_pCommandLine += "{}\n"_f << _HostInfo.f_GetDesc();
				else
					*_pCommandLine += "{}\n"_f << _HostInfo.m_HostID;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetConnectionConcurrency
		(
			 NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			 , NStr::CStr const &_URL
			 , int32 _ConnectionConcurrency
		)
	{
		TCPromise<uint32> Promise;

		if (!fg_ValidateConnectionConcurrency(_ConnectionConcurrency, Promise))
			return Promise.f_MoveFuture();

		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetClientConnectionConcurrency, Address, _ConnectionConcurrency)
			> Promise / [Promise, Address, _ConnectionConcurrency]()
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
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
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
	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
	{
		TCPromise<uint32> Promise;
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddListen, Address)
			> Promise / [this, Promise, _URL, Address]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' from command line", _URL);

				auto EncodedURL = Address.m_URL.f_Encode();

				bool bPrimary = fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, false);

				mp_State.m_ConfigDatabase.f_Save() > Promise / [this, Promise, Address, bPrimary, _URL]
					{
						if (bPrimary)
						{
							mp_PrimaryListen = Address;
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Add primary listen '{}' saved to database from command line", _URL);
						}
						else
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Add listen '{}' saved to database from command line", _URL);
						Promise.f_SetResult(0);
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
	{
		TCPromise<uint32> Promise;
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveListen, Address)
			> Promise / [this, Promise, _URL, Address]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' from command line", _URL);

				auto EncodedURL = Address.m_URL.f_Encode();

				CStr NewPrimary = fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);

				mp_State.m_ConfigDatabase.f_Save() > Promise / [this, Promise, Address, NewPrimary, _URL]
					{
						if (NewPrimary != mp_PrimaryListen.m_URL.f_Encode())
						{
							mp_PrimaryListen.m_URL = NewPrimary;
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database and new primary listen set to '{}' from command line", _URL, NewPrimary);
						}
						else
							DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove listen '{}' saved to database from command line", _URL);
						Promise.f_SetResult(0);
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetPrimaryListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, NStr::CStr const &_URL)
	{
		TCPromise<uint32> Promise;
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _URL;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_HasListen, Address)
			> Promise / [this, Promise, _URL, Address](bool _bHasListen)
			{
				if (!_bHasListen)
				{
					Promise.f_SetException(DMibErrorInstance("Not listening to this address, so can't sett as primary listen."));
					return;
				}

				auto EncodedURL = Address.m_URL.f_Encode();

				fg_RemoveListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL);
				fg_AddListen(mp_State.m_ConfigDatabase.m_Data, EncodedURL, true);

				mp_State.m_ConfigDatabase.f_Save() > Promise / [this, Promise, Address, EncodedURL]
					{
						DMibLogWithCategory(Mib/Concurrency/App, Info, "Set primary listen '{}' from command line", EncodedURL);
						mp_PrimaryListen = Address;
						Promise.f_SetResult(0);
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListListen(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumListens)
			> Promise / [=](NContainer::TCSet<CDistributedActorTrustManager_Address> &&_Listens)
			{
				CStr Result;
				for (auto &Listen : _Listens)
					Result += "{}\n"_f << Listen.m_URL.f_Encode();
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");
				*_pCommandLine += Result;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListNamespaces(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeTrustedHosts)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumNamespacePermissions, _bIncludeTrustedHosts)
			> Promise / [=](NContainer::TCMap<NStr::CStr, CDistributedActorTrustManager::CNamespacePermissions> &&_Namespaces)
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
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_TrustHostForNamespace
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NStr::CStr const &_Namespace
			, NStr::CStr const &_Host
		)
	{
		TCPromise<uint32> Promise;
		NContainer::TCSet<NStr::CStr> Hosts;
		Hosts[_Host];
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AllowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
			> Promise / [Promise, _Namespace, _Host]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Trusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_UntrustHostForNamespace
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NStr::CStr const &_Namespace
			, NStr::CStr const &_Host
		)
	{
		TCPromise<uint32> Promise;
		NContainer::TCSet<NStr::CStr> Hosts;
		Hosts[_Host];
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_DisallowHostsForNamespace, _Namespace, Hosts, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions)
			> Promise / [Promise, _Namespace, _Host]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Untrusted host '{}' for namespace '{}' from command line", _Host, _Namespace);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}


	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListPermissions(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, bool _bIncludeHosts)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumPermissions, _bIncludeHosts)
			> Promise / [=](CDistributedActorTrustManagerInterface::CEnumPermissionsResult &&_Permissions)
			{
				CStr Result;
				for (auto &Permission : _Permissions.m_Permissions)
				{
					auto &PermissionName = _Permissions.m_Permissions.fs_GetKey(Permission);
					Result += "{}\n"_f << PermissionName;
					if (_bIncludeHosts)
					{
						for (auto &PermissionInfo : Permission)
							Result += "    {}\n"_f << PermissionInfo.f_GetDesc();
					}
				}
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported permissions to command line {}", Result);
				*_pCommandLine += Result;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddPermission
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NStr::CStr const &_HostID
			, NStr::CStr const &_UserID
			, NStr::CStr const &_Permission
			, CEJSON const &_AuthenticationFactors
			, int64 _AuthenticationLifetime
		)
	{
		if (_HostID.f_IsEmpty() && _UserID.f_IsEmpty())
			return DMibErrorInstance("You need to specify at least one of --host or --user");

		TCPromise<uint32> Promise;
		NContainer::TCMap<NStr::CStr, CPermissionRequirements> Permissions;
		Permissions[_Permission] = fg_ParsePermissionRequirements(_AuthenticationFactors);

		mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_AddPermissions
				, CPermissionIdentifiers{_HostID, _UserID}
				, Permissions
				, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
			)
			> Promise / [Promise, _HostID, _UserID, _Permission, _AuthenticationFactors]()
			{
				DMibLogWithCategory
					(
						Mib/Concurrency/App
						, Info
						, "Add permission '{}' to host '{}' user '{}' authentication factor '{}' from command line"
						, _Permission
						, _HostID
						, _UserID
						, _AuthenticationFactors
					)
				;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemovePermission
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, NStr::CStr const &_HostID
			, NStr::CStr const &_UserID
			, NStr::CStr const &_Permission
		)
	{
		TCPromise<uint32> Promise;
		NContainer::TCSet<NStr::CStr> Permissions;
		Permissions[_Permission];
		mp_State.m_TrustManager
			(
				&CDistributedActorTrustManager::f_RemovePermissions
				, CPermissionIdentifiers{_HostID, _UserID}
				, Permissions
				, EDistributedActorTrustManagerOrderingFlag_WaitForSubscriptions
			)
			> Promise / [Promise, _HostID, _UserID, _Permission]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove permission '{}' from host '{}' user '{}' from command line", _Permission, _HostID, _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
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

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		TCPromise<uint32> Promise;
		bool bQuiet = _Params["Quiet"].f_Boolean();
		CStr const &_UserName = _Params["UserName"].f_String();
		CStr const &UserID = NCryptography::fg_RandomID();
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddUser, UserID, _UserName)
			> Promise / [=]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Add user ID '{}' ({}) to host from command line", UserID, _UserName);
				if (!bQuiet)
					*_pCommandLine += "{}\n"_f << UserID;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveUser, _UserID)
			> Promise / [Promise, _UserID]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove user '{}' from command line", _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListUsers(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUsers, false)
			> Promise / [=](NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> &&_Users)
			{
				CStr Result;
				for (auto &UserInfo : _Users)
				{
					auto &UserID = _Users.fs_GetKey(UserInfo);
					Result += "{} {}\n"_f << UserID << UserInfo.m_UserName;
				}
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported users to command line {}", Result);
				*_pCommandLine += Result;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetUserInfo(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		TCPromise<uint32> Promise;
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
			> Promise / [Promise, UserID]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Set metadata for user '{}' from command line", UserID);
				Promise.f_SetResult(0);
			}
		;

		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveMetadata(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, CStr const &_Key)
	{
		TCPromise<uint32> Promise;
		TCOptional<CStr> UserName;
		TCMap<CStr, CEJSON> AddMetadata;
		TCSet<CStr> RemoveMetadata;
		RemoveMetadata[_Key];
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo, _UserID, UserName, AddMetadata, RemoveMetadata)
			> Promise / [Promise, _UserID, _Key]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove metadata '{}' from user '{}' from command line", _Key, _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ExportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, bool _bIncludePrivate)
	{
		TCPromise<uint32> Promise;
		fg_ExportUser(mp_State.m_TrustManager, _UserID, _bIncludePrivate) > Promise / [Promise, _UserID, _pCommandLine](CStr const &_UserData)
			{
				*_pCommandLine += "{}\n"_f << _UserData;
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Exported user ID '{}' from command line", _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ImportUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserData)
	{
		TCPromise<uint32> Promise;
		fg_ImportUser(mp_State.m_TrustManager, _UserData) > Promise / [Promise](CStr const &_UserID)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Import user ID '{}' from command line", _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser)
			> Promise / [Promise, _pCommandLine](CStr const &_UserID)
			{
				*_pCommandLine += "{}\n"_f << _UserID;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetDefaultUser(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, _UserID)
			> Promise / [Promise]()
			{
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RegisterAuthenticationFactor
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_UserID
			, CStr const &_Factor
			, bool _bQuiet
		)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, _pCommandLine, _UserID, _Factor)
			> Promise / [=](CStr const &_FactorID)
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Added authentication factor '{}' of type '{}' to user '{}' from command line", _FactorID, _Factor, _UserID);
				if (!_bQuiet)
					*_pCommandLine += "{}\n"_f << _FactorID;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_UnregisterAuthenticationFactor
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_UserID
			, CStr const &_Factor
		)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, _UserID, _Factor)
			> Promise / [Promise, _UserID, _Factor]()
			{
				DMibLogWithCategory(Mib/Concurrency/App, Info, "Removed authentication factor '{}' from user '{}' from command line", _Factor, _UserID);
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_EnumUserAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _UserID)
			> Promise / [Promise, _UserID, _pCommandLine](TCMap<CStr, CAuthenticationData> &&_Factors)
			{
				CStr Result;
				for (auto &Factor : _Factors)
					Result += "{} {}\n"_f << _Factors.fs_GetKey(Factor) << Factor.m_Name;
				*_pCommandLine += Result;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_EnumAuthenticationFactors(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		TCPromise<uint32> Promise;
		mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			> Promise / [Promise, _pCommandLine](NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo> &&_Factors)
			{
				CStr Result;
				for (auto &Factor : _Factors)
					Result += "{}\n"_f << _Factors.fs_GetKey(Factor);
				*_pCommandLine += Result;
				Promise.f_SetResult(0);
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AuthenticatePermissionPattern(NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		TCPromise<uint32> Promise;

		CStr Pattern = _Params["Pattern"].f_String();
		bool bJSONOutput = _Params["JSONOutput"].f_Boolean();

		TCSet<CStr> Factors;
		CEJSON const &AuthenticationFactors = _Params["AuthenticationFactors"];
		if (AuthenticationFactors.f_IsString())
			Factors[AuthenticationFactors.f_String()];
		else
		{
			for (auto &Factor : AuthenticationFactors.f_Array())
				Factors[Factor.f_String()];
		}

		struct CCollectedAuthenticationActorInfo
		{
			TCDistributedActor<ICDistributedActorAuthentication> m_Actor;
			CStr m_RemoteHostID;
			CStr m_Description;
		};

		TCVector<CCollectedAuthenticationActorInfo> AuthenticationActors;
		for (auto const &RegisteredAuthentication : mp_AuthenticationRegistrationSubscriptions)
		{
			auto const &WeakActor = mp_AuthenticationRegistrationSubscriptions.fs_GetKey(RegisteredAuthentication);

			auto Actor = WeakActor.f_Lock();
			if (!Actor)
				continue;
			AuthenticationActors.f_Insert({Actor, RegisteredAuthentication.m_ActorInfo.m_HostInfo.m_HostID, RegisteredAuthentication.m_ActorInfo.m_HostInfo.f_GetDesc()});
		}

		mp_AuthenticationHandlerImplementation(&ICDistributedActorAuthenticationHandler::f_GetMultipleRequestSubscription, AuthenticationActors.f_GetLen())
			+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			> [=]
			(
				TCAsyncResult<ICDistributedActorAuthenticationHandler::CMultipleRequestData> &&_MultipleRequestData
				, TCAsyncResult<NContainer::TCMap<NStr::CStr, CAuthenticationActorInfo>> &&_Factors
			) mutable
			{
				if (!_MultipleRequestData)
					return Promise.f_SetException(_MultipleRequestData.f_GetException());
				if (!_Factors)
					return Promise.f_SetException(_Factors.f_GetException());

				for (auto const &Factor : Factors)
				{
					if (!_Factors->f_FindEqual(Factor))
						return Promise.f_SetException(DMibErrorInstance("No authentication factor named '{}'"_f << Factor));
				}

				TCActorResultVector<bool> Results;
				for (auto const &AuthenticationActorInfo : AuthenticationActors)
				{
					AuthenticationActorInfo.m_Actor.f_CallActor(&ICDistributedActorAuthentication::f_AuthenticatePermissionPattern)
						(
							Pattern
							, Factors
							, _MultipleRequestData->m_ID
						)
						.f_Timeout(10.0, "Timeout wating for manager to reply")
						> Results.f_AddResult()
					;
				}

				Results.f_GetResults() > Promise / [=, MultipleRequestSubscription = fg_Move(_MultipleRequestData->m_Subscription)](TCVector<TCAsyncResult<bool>> &&_Results)
					{
						aint ReturnValue = 0;
						DMibCheck(AuthenticationActors.f_GetLen() == _Results.f_GetLen());

						CEJSON JSONOutput;

						if (bJSONOutput)
							JSONOutput["AuthenticationFailures"] = EJSONType_Array;

						auto iAuthenticationActorInfo = AuthenticationActors.f_GetIterator();
						for (auto &Result : _Results)
						{
							auto &AuthenticationActorInfo = *iAuthenticationActorInfo;
							++iAuthenticationActorInfo;

							if (!Result)
							{
								if (bJSONOutput)
								{
									JSONOutput["AuthenticationFailures"].f_Array().f_Insert() =
										{
											"RemoteHostID"_= AuthenticationActorInfo.m_RemoteHostID
											, "RemoteHostDescription"_= AuthenticationActorInfo.m_Description
											, "Exception"_= Result.f_GetExceptionStr()
										}
									;
								}
								else
									*_pCommandLine %= "[{}] Failed authenticate pattern. Exception: {}\n"_f << AuthenticationActorInfo.m_Description << Result.f_GetExceptionStr();

								ReturnValue = 1;
								continue;
							}

							if (!*Result)
							{
								if (bJSONOutput)
								{
									JSONOutput["AuthenticationFailures"].f_Array().f_Insert() =
										{
											"RemoteHostID"_= AuthenticationActorInfo.m_RemoteHostID
											, "RemoteHostDescription"_= AuthenticationActorInfo.m_Description
										}
									;
								}
								else
									*_pCommandLine %= "[{}] Failed to authenticate.\n"_f << AuthenticationActorInfo.m_Description;
								ReturnValue = 1;
							}
						}
						if (bJSONOutput)
							*_pCommandLine += JSONOutput.f_ToString();

						Promise.f_SetResult(ReturnValue);
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}

	TCFuture<uint32> CDistributedAppActor::fp_RunCommandLineAndLogError
		(
			CStr const &_Description
			, NFunction::TCFunction<TCFuture<uint32> ()> &&_fCommand
		)
	{
		TCPromise<uint32> Promise;
		fg_Dispatch
			(
				[fCommand = fg_Move(_fCommand)]
				{
					return fCommand();
				}
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

	NCommandLine::EAnsiEncodingFlag CDistributedAppActor::fs_ColorAnsiFlagsDefault()
	{
		NCommandLine::EAnsiEncodingFlag Flags = NCommandLine::EAnsiEncodingFlag_None;
		if (fs_ColorEnabledDefault())
			Flags |= NCommandLine::EAnsiEncodingFlag_Color;
		if (fs_Color24BitEnabledDefault())
			Flags |= NCommandLine::EAnsiEncodingFlag_Color24Bit;
		if (fs_ColorLightBackgroundDefault())
			Flags |= NCommandLine::EAnsiEncodingFlag_ColorLightBackground;
		return Flags;
	}

	bool CDistributedAppActor::fs_ColorEnabledDefault()
	{
		static bool bValue = []
			{
				bool bDefaultEnableColor = !NSys::fg_System_BeingDebugged();
				return fg_GetSys()->f_GetEnvironmentVariable("MalterlibColor", bDefaultEnableColor ? "true" : "false") == "true";
			}
			()
		;
		return bValue;
	}

	bool CDistributedAppActor::fs_Color24BitEnabledDefault()
	{
		static bool bValue = []
			{
				if (auto Value = fg_GetSys()->f_GetEnvironmentVariable("MalterlibColor24Bit", ""))
					return Value == "true";

				auto ColorTerm = fg_GetSys()->f_GetEnvironmentVariable("COLORTERM", "");
				if (ColorTerm == "truecolor" || ColorTerm == "24bit")
					return true;

				return false;
			}
			()
		;
		return bValue;
	}

	bool CDistributedAppActor::fs_ColorLightBackgroundDefault()
	{
		static bool bValue = []
			{
				if (auto Value = fg_GetSys()->f_GetEnvironmentVariable("MalterlibColorLight", ""); Value == "true")
					return true;
				else if (Value != "auto")
					return false;

				struct CState
				{
					NThread::CEvent m_Event;
					NStr::CStr m_Buffer;
					bool m_bLight = false;
				};

				NStorage::TCSharedPointer<CState> pState = fg_Construct();

				auto Params = NProcess::CStdInReaderParams::fs_Create
					(
						[pState](EStdInReaderOutputType _Type, NStr::CStrSecure const &_Input)
						{
							if (_Type != EStdInReaderOutputType_StdIn)
								return;
							CByteVector Data((uint8 const *)_Input.f_GetStr(), _Input.f_GetLen());

							DMibConOut("Input: {vs}\n", CByteVector((uint8 const *)_Input.f_GetStr(), _Input.f_GetLen()));

							pState->m_Buffer += _Input;
							if (auto iFound = pState->m_Buffer.f_Find("\x1B]11;rgb:"); iFound >= 0)
							{
								CStr Buffer = pState->m_Buffer.f_Extract(iFound);

								uint8 Red;
								uint8 Green;
								uint8 Blue;
								aint nParsed;
								(CStr::CParse("\x1B]11;rgb:{nfh}/{nfh}/{nfh}\x07") >> Red >> Green >> Blue).f_Parse(Buffer, nParsed);
								if (nParsed == 3)
								{
									pState->m_bLight = (fp32(Red) * 0.3f + fp32(Green) * 0.59f + fp64(Blue) * 0.11f) > 128.0f;
									pState->m_Event.f_SetSignaled();
								}
							}
						}
					)
				;

				NProcess::CStdInReader StdIn(fg_Move(Params));

				DMibConOut2("\x1B]11;?\x1B\\");

				if (pState->m_Event.f_WaitTimeout(1.0))
					return false;

				return pState->m_bLight;
			}
			()
		;
		return bValue;
	}

	void CDistributedAppActor::fp_BuildDefaultCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine, EDefaultCommandLineFunctionality _Functionalities)
	{
		o_CommandLine.f_RegisterGlobalOptions
			(
				{
					"MalterlibCommand?"_=
					{
						"Names"_= {"--malterlib-command-BA49ADC8-6CAA-4E8C-BA13-3A9273859D89"}
						, "Type"_= ""
						, "Description"_= "Override the command to run anywhere on the command line."
						, "Hidden"_= true
					}
				}
			)
		;

		if (_Functionalities & EDefaultCommandLineFunctionality_Color)
		{
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"Color?"_=
						{
							"Names"_= {"--color"}
							, "Default"_= fs_ColorEnabledDefault()
							, "Description"_= "Display text output with ansi colors where supported.\n"\
							"You can override behaviour with MalterlibColor env var. Set it to true or false. "\
						}
					}
				)
			;

			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"Color24Bit?"_=
						{
							"Names"_= {"--color-24bit"}
							, "Default"_= fs_Color24BitEnabledDefault()
							, "Description"_= "Display text output with 24 bit ansi colors.\n"\
							"By default detected through COLORTERM. "\
							"You can override behaviour with MalterlibColor24Bit env var. Set it to true or false. "\
						}
					}
				)
			;

			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"ColorLight?"_=
						{
							"Names"_= {"--color-light"}
							, "Default"_= fs_ColorLightBackgroundDefault()
							, "Description"_= "Force light background.\n"\
							"You can override behaviour with MalterlibColorLight env var. Set it to true, false or auto. "\
							"With auto a xterm secquence will be used to determine background color. "\
							"Can interfere with commands that use std input processing."
						}
					}
				)
			;
		}

		if (_Functionalities & EDefaultCommandLineFunctionality_Help)
			o_CommandLine.f_AddHelpCommand();

		if (_Functionalities & EDefaultCommandLineFunctionality_Logging)
		{
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"ConcurrentLogging?"_=
						{
							"Names"_= {"--log-on-thread"}
							, "Default"_= true
							, "Description"_= "Log on a separate thread to minimally affect application performance and behavior."
						}
						, "StdErrLogger?"_=
						{
							"Names"_= {"--log-to-stderr"}
							, "Default"_= false
							, "Description"_= "Log to std error."
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
							, "Description"_= "Log to trace."
						}
					}
				)
			;
#endif
		}

		if (_Functionalities & EDefaultCommandLineFunctionality_Authentication)
		{
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"AuthenticationLifetime?"_=
						{
							"Names"_= {"--authentication-lifetime"}
							,"Default"_= CPermissionRequirements::mc_OverrideLifetimeNotSet
							, "Description"_= "The number of minutes until the authentication expires when cached."
							, "ValidForDirectCommand"_= false
						}
					}
				)
			;
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"AuthenticationUser?"_=
						{
							"Names"_= {"--authentication-user"}
							,"Type"_= ""
							, "Description"_= "Override the default user ID used for authentication."
							, "ValidForDirectCommand"_= false
						}
					}
				)
			;
		}

		if (_Functionalities & EDefaultCommandLineFunctionality_DistributedComputing)
		{
			auto Distributed = o_CommandLine.f_AddSection("Distributed Computing", "Use these commands to manage connectability and trust between different hosts.");

			auto IncludeFriendlyNameOption = "IncludeFriendlyName?"_=
				{
					"Names"_= {"--include-friendly-name"}
					, "Default"_= true
					, "Description"_= "Include friendly host name in output."
				}
			;

			auto ConnectionConcurrencyOption = "ConnectionConcurrency?"_=
				{
					"Names"_= {"--connection-concurrency"}
					, "Default"_= -1
					, "Description"_= "The number of parallel connections to connect to address with. Useful for increasing network throughput. Set to -1 to use default."
				}
			;

			auto PermissionUserAuthenticationFactorsOption = "AuthenticationFactors?"_=
				{
					"Names"_= {"--authentication-factors"}
					, "Type"_= COneOfType{CEJSON({CEJSON({""})}), CEJSON({""}), CEJSON{""}}
					, "Description"_= "The authentication factor(s) used for the permission. Factors must be specified as a string or a JSON array (of arrays):\n"
						"\"Factor1\"                                        - must authenticate by Factor1\n"
						"[\"Factor1\", \"Factor2\"]                           - must authenticate by Factor1 or Factor2\n"
						"[[\"Factor1\"], [\"Factor2\"]]                       - must authenticate by Factor1 and Factor2\n"
						"[[\"Factor1\", \"Factor2\"], [\"Factor1\", \"Factor3\"]] - must authenticate by Factor1 and either Factor2 or Factor3.\n"
				}
			;

			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-host-id"}
						, "Description"_= "Get the host ID for this application.\n"
						, "Output"_= "The host ID of this application."
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_GetHostID(_pCommandLine);
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_GetConnetionStatus(_pCommandLine);
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
							"Ticket?"_=
							{
								"Default"_= ""
								, "Description"_= "Specify ticket to use for adding the connection, if this is empty, the ticket will be promted for."
							}
						}
						, "Options"_=
						{
							ConnectionConcurrencyOption
							, IncludeFriendlyNameOption
							, "TrustedNamespaces?"_=
							{
								"Names"_= {"--trusted-namespaces"}
								, "Type"_= {""}
								, "Default"_= _[_]
								, "Description"_= "The namespaces that should be trusted for the host you are establishing trust with."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add trust connection"
								, [=]
								{
									NContainer::TCSet<NStr::CStr> TrustedNamespaces;
									for (auto &NamespaceJSON : _Params["TrustedNamespaces"].f_Array())
										TrustedNamespaces[NamespaceJSON.f_String()];

									return f_CommandLine_AddConnection
										(
											_pCommandLine
											, _Params["Ticket"].f_String()
											, _Params["IncludeFriendlyName"].f_Boolean()
											, _Params["ConnectionConcurrency"].f_Integer()
											, fg_Move(TrustedNamespaces)
										)
									;
								}
							)
						;
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
								, "Description"_= "Specify ticket to use for adding the connection."
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The URL of the connection you want to add."
							}
						}
						, "Options"_=
						{
							ConnectionConcurrencyOption
							, IncludeFriendlyNameOption
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-connection-remove"}
						, "Category"_= Category
						, "Description"_= "Remove an outgoing connection.\n"
						, "Parameters"_=
						{
							"ConnectionURL"_=
							{
								"Type"_= ""
								, "Description"_= "The URL of the connection you want to remove."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The host ID of the host you want to remove trust for."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
						, "Options"_=
						{
							"Permissions?"_=
							{
								"Names"_= {"--permissions"}
								, "Type"_= {""}
								, "Default"_= _[_]
								, "Description"_= "The permissions that should be granted to the host/user that uses the returned ticket."
							}
							, "UserID?"_=
							{
								"Names"_= {"--user"}
								, "Type"_= ""
								, "Description"_= "If specified, the permissions are added for the specific user for the host that uses the ticket."
							}
							, PermissionUserAuthenticationFactorsOption
						}
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Generate trust ticket"
								, [=]
								{
									NContainer::TCSet<NStr::CStr> Permissions;
									for (auto &PermissionJSON : _Params["Permissions"].f_Array())
										Permissions[PermissionJSON.f_String()];

									CStr UserID;
									if (auto *pValue =_Params.f_GetMember("UserID"))
										UserID = pValue->f_String();

									CEJSON AuthenticationFactors;
									if (auto *pValue =_Params.f_GetMember("AuthenticationFactors"))
										AuthenticationFactors = pValue->f_Array();

									return f_CommandLine_GenerateTrustTicket(_pCommandLine, _Params["ForListenURL"].f_String(), Permissions, UserID, AuthenticationFactors);
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "Include trusted and untrusted hosts in output."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
			Category = "Host and user permissions";
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
								, "Description"_= "Include the hosts that have the permissions in the output."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_ListPermissions(_pCommandLine, _Params["IncludeHosts"].f_Boolean());
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-permission-add"}
						, "Category"_= Category
						, "Description"_= "Add a permission to a host, a user, or a host/user pair.\n"
						, "Parameters"_=
						{
							"Permission"_=
							{
								"Type"_= ""
								, "Description"_= "The permission to add."
							}
						}
						, "Options"_=
						{
							"HostID?"_=
							{
								"Names"_= {"--host"}
								, "Type"_= ""
								, "Description"_= "The host to add a permission to."
							}
							, "UserID?"_=
							{
								"Names"_= {"--user"}
								, "Type"_= ""
								, "Description"_= "The user to add the permission to."
							}
							, "MaxLifetime?"_=
							{
								"Names"_= {"--max-lifetime"}
								,"Default"_= CPermissionRequirements::mc_DefaultMaximumLifetime
								, "Description"_= "The maximum number of minutes that a cached authentication can be used for this granted permission."
							}
							, PermissionUserAuthenticationFactorsOption
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add permissions"
								, [=]
								{
									CStr HostID;
									if (auto *pValue =_Params.f_GetMember("HostID"))
										HostID = pValue->f_String();
									CStr UserID;
									if (auto *pValue =_Params.f_GetMember("UserID"))
										UserID = pValue->f_String();
									CEJSON AuthenticationFactors;
									if (auto *pValue =_Params.f_GetMember("AuthenticationFactors"))
										AuthenticationFactors = pValue->f_Array();
									int64 Lifetime = _Params["MaxLifetime"].f_Integer();
									return f_CommandLine_AddPermission(_pCommandLine, HostID, UserID, _Params["Permission"].f_String(), AuthenticationFactors, Lifetime);
								}
							)
						;
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-permission-remove"}
						, "Category"_= Category
						, "Description"_= "Remove a permission from a host, a user, or a host/user pair.\n"
						, "Parameters"_=
						{
							"Permission"_=
							{
								"Type"_= ""
								, "Description"_= "The permission to remove."
							}
						}
						, "Options"_=
						{
							"HostID?"_=
							{
								"Names"_= {"--host"}
								, "Type"_= ""
								, "Description"_= "The host to remove the permission from."
							}
							, "UserID?"_=
							{
								"Names"_= {"--user"}
								, "Type"_= ""
								, "Description"_= "The user to remove the permission from."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Remove permissions"
								, [=]
								{
									CStr HostID;
									if (auto *pValue =_Params.f_GetMember("HostID"))
										HostID = pValue->f_String();
									CStr UserID;
									if (auto *pValue =_Params.f_GetMember("UserID"))
										UserID = pValue->f_String();

									return f_CommandLine_RemovePermission
										(
											_pCommandLine
											, HostID
											, UserID
											, _Params["Permission"].f_String()
										)
									;
								}
							)
						;
					}
				)
			;
			Category = "User management";
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-user-add"}
						, "Category"_= Category
						, "Description"_= "Add a user to the database.\n"
						, "Output"_= "None."
						, "Options"_=
						{
							"Quiet?"_=
							{
								"Names"_= {"--quiet"}
								, "Default"_= false
								, "Description"_= "Don't output user ID."
							}
						}
						, "Parameters"_=
						{
							"UserName"_=
							{
								"Type"_= ""
								, "Description"_= "The name of the user being added to the database."
							}
						}
				}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The user ID to remove from the database."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The new username."
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "Key of the metadata to remove."
							}
						}
						, "Parameters"_=
						{
							"UserID"_=
							{
								"Type"_= ""
								, "Description"_= "The user ID to remove metadata from."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_RemoveMetadata(_pCommandLine, _Params["UserID"].f_String(), _Params["Key"].f_String());
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-user-get-default-user"}
						, "Category"_= Category
						, "Description"_= "Get the default user name.\n"
						, "Output"_= "The default username."
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_GetDefaultUser(_pCommandLine);
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-user-set-default-user"}
						, "Category"_= Category
						, "Description"_= "Set the user name to use.\n"
						, "Output"_= "None."
						, "Parameters"_=
						{
							"UserID"_=
							{
								"Type"_= ""
								, "Description"_= "The user ID to use."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_SetDefaultUser(_pCommandLine, _Params["UserID"].f_String());
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
								, "Default"_= false
								, "Description"_= "In addition to data needed to verify a users identity, include private data needed to authenticate as that user."
							}
						}
						, "Parameters"_=
						{
							"UserID"_=
							{
								"Type"_= ""
								, "Description"_= "The user ID of the user to export."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The encoded text representation of the user data."
							}
						}
				}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
								, "Description"_= "The authentication factor to add."
							}
							, "Quiet?"_=
							{
								"Names"_= {"--quiet"}
								, "Default"_= false
								, "Description"_= "Don't output factor ID."
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_RegisterAuthenticationFactor(_pCommandLine, _Params["UserID"].f_String(), _Params["Factor"].f_String(), _Params["Quiet"].f_Boolean());
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
								, "Description"_= "The authentication factor to remove."
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
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
						, "Description"_= "List available authentication factors.\n"
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
						, "Description"_= "List the authentication factors registered to a user.\n"
						, "Parameters"_=
						{
							"UserID"_=
							{
								"Type"_= ""
								, "Description"_= "List the authentication factors registered for the user."
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_EnumUserAuthenticationFactors(_pCommandLine, _Params["UserID"].f_String());
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-user-authenticate-pattern"}
						, "Category"_= Category
						, "Description"_= "Give advance authentication for all permissions that matches the pattern.\n"
						, "Options"_=
						{
							"AuthenticationFactors"_=
							{
								"Names"_= {"--authentication-factors"}
								, "Type"_= COneOfType{CEJSON({""}), CEJSON{""}}
								, "Description"_= "The factor(s) used for authentication.\n"
								"Authenticate these factors in advance.\n"
							}
							, "JSONOutput?"_=
							{
								"Names"_= {"--json-output"}
								, "Default"_= false
								, "Description"_= "Return authentication failures/successes in EJSON format."
							}
						}
						, "Parameters"_=
						{
							"Pattern"_=
							{
								"Type"_= ""
								, "Description"_= "Pattern for matching permissions.\n"
								"All permissions matching this pattern will preauthenticated with the listed factors\n"
							}
						}
					}
					, [this](CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return f_CommandLine_AuthenticatePermissionPattern(_pCommandLine, _Params);
					}
				)
			;
		}
	}
}
