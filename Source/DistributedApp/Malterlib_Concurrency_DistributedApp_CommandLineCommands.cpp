// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NEncoding;
	using namespace NCommandLine;
	using namespace NStorage;
	using namespace NContainer;

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
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"BoxDrawing?"_=
						{
							"Names"_= {"--box-drawing"}
							, "Default"_= fs_BoxDrawingDefault()
							, "Description"_= "Enable box drawing characters.\n"\
						}
					}
				)
			;
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"TerminalWidth?"_=
						{
							"Names"_= {"--terminal-width"}
							, "Default"_= fg_GetSys()->f_GetEnvironmentVariable("MalterlibTerminalWidth", "-1").f_ToInt(int64(-1))
							, "Description"_= "Override detected terminal width.\n"\
							"Useful for when for whatever reason tty is not accessible. "\
						}
					}
				)
			;
			o_CommandLine.f_RegisterGlobalOptions
				(
					{
						"TerminalHeight?"_=
						{
							"Names"_= {"--terminal-height"}
							, "Default"_= fg_GetSys()->f_GetEnvironmentVariable("MalterlibTerminalHeight", "-1").f_ToInt(int64(-1))
							, "Description"_= "Override detected terminal height.\n"\
							"Useful for when for whatever reason tty is not accessible. "\
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
						"[[\"Factor1\", \"Factor2\"]]                         - must authenticate by Factor1 and Factor2\n"
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetHostID, _pCommandLine);
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
					, [this](CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)
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
							, CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_ListConnections
							 	, _pCommandLine
							 	, _Params["IncludeFriendlyName"].f_Boolean()
							 	, _Params["TableType"].f_String()
							)
						;
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
						, "Options"_=
						{
							CTableRenderHelper::fs_OutputTypeOption()
						}

					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetConnetionStatus, _pCommandLine, _Params["TableType"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add trust connection"
								, [=]() -> TCFuture<uint32>
								{
									TCSet<CStr> TrustedNamespaces;
									for (auto &NamespaceJSON : _Params["TrustedNamespaces"].f_Array())
										TrustedNamespaces[NamespaceJSON.f_String()];

									return g_Future <<= self
										(
											&CDistributedAppActor::f_CommandLine_AddConnection
											, _pCommandLine
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Set connection concurrency"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self
										(
											&CDistributedAppActor::f_CommandLine_SetConnectionConcurrency
											, _pCommandLine
											, _Params["Address"].f_String()
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add additional trust connection"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self
										(
											&CDistributedAppActor::f_CommandLine_AddAdditionalConnection
											, _pCommandLine
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Remove trust connection"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveConnection, _pCommandLine, _Params["ConnectionURL"].f_String());
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
							, CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_ListTrustedHosts
							 	, _pCommandLine
							 	, _Params["IncludeFriendlyName"].f_Boolean()
							 	, _Params["TableType"].f_String()
							)
						;
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Remove trusted host"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveTrustedHost, _pCommandLine, _Params["HostID"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Generate trust ticket"
								, [=]() -> TCFuture<uint32>
								{
									TCSet<CStr> Permissions;
									for (auto &PermissionJSON : _Params["Permissions"].f_Array())
										Permissions[PermissionJSON.f_String()];

									CStr UserID;
									if (auto *pValue =_Params.f_GetMember("UserID"))
										UserID = pValue->f_String();

									CEJSON AuthenticationFactors;
									if (auto *pValue =_Params.f_GetMember("AuthenticationFactors"))
										AuthenticationFactors = pValue->f_Array();

									return g_Future <<= self
										(
										 	&CDistributedAppActor::f_CommandLine_GenerateTrustTicket
										 	, _pCommandLine
										 	, _Params["ForListenURL"].f_String()
										 	, Permissions
										 	, UserID
										 	, AuthenticationFactors
										)
									;
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
						, "Options"_=
						{
							CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ListListen, _pCommandLine, _Params["TableType"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add trust listen address"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AddListen, _pCommandLine, _Params["ListenURL"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Remove listen address"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveListen, _pCommandLine, _Params["ListenURL"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Set primary listen address"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self(&CDistributedAppActor::f_CommandLine_SetPrimaryListen, _pCommandLine, _Params["ListenURL"].f_String());
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
							, CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_ListNamespaces
							 	, _pCommandLine
							 	, _Params["IncludeTrustedHosts"].f_Boolean()
							 	, _Params["TableType"].f_String()
							)
						;
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Trusted host for namespace"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self
										(
										 	&CDistributedAppActor::f_CommandLine_TrustHostForNamespace
										 	, _pCommandLine
										 	, _Params["TrustNamespace"].f_String()
										 	, _Params["TrustHost"].f_String()
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Untrust host for namespace"
								, [=]() -> TCFuture<uint32>
								{
									return g_Future <<= self
										(
										 	&CDistributedAppActor::f_CommandLine_UntrustHostForNamespace
										 	, _pCommandLine
										 	, _Params["TrustNamespace"].f_String()
										 	, _Params["TrustHost"].f_String()
										)
									;
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
							"IncludeTargets?"_=
							{
								"Names"_= {"--include-targets"}
								, "Default"_= true
								, "Description"_= "Include the targets (hosts/users) that have the permissions in the output."
							}
							, CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_ListPermissions
							 	, _pCommandLine
							 	, _Params["IncludeTargets"].f_Boolean()
							 	, _Params["TableType"].f_String()
							)
						;
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Add permissions"
								, [=]() -> TCFuture<uint32>
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
									return g_Future <<= self
										(
										 	&CDistributedAppActor::f_CommandLine_AddPermission
										 	, _pCommandLine
										 	, HostID
										 	, UserID
										 	, _Params["Permission"].f_String()
										 	, AuthenticationFactors
										 	, Lifetime
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return fp_RunCommandLineAndLogError
							(
								"Remove permissions"
								, [=]() -> TCFuture<uint32>
								{
									CStr HostID;
									if (auto *pValue =_Params.f_GetMember("HostID"))
										HostID = pValue->f_String();
									CStr UserID;
									if (auto *pValue =_Params.f_GetMember("UserID"))
										UserID = pValue->f_String();

									return g_Future <<= self
										(
											&CDistributedAppActor::f_CommandLine_RemovePermission
											, _pCommandLine
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AddUser, _pCommandLine, _Params);
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveUser, _pCommandLine, _Params["UserID"].f_String());
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
						, "Options"_=
						{
							CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ListUsers, _pCommandLine, _Params["TableType"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_SetUserInfo, _pCommandLine, _Params);
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_RemoveMetadata
							 	, _pCommandLine
							 	, _Params["UserID"].f_String()
							 	, _Params["Key"].f_String()
							)
						;
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetDefaultUser, _pCommandLine);
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_SetDefaultUser, _pCommandLine, _Params["UserID"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						bool bIncludePrivate = false;
						if (auto *pValue = _Params.f_GetMember("IncludePrivate"))
						{
							bIncludePrivate = pValue->f_Boolean();
						}

						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ExportUser, _pCommandLine, _Params["UserID"].f_String(), bIncludePrivate);
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ImportUser, _pCommandLine, _Params["UserData"].f_String());
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_RegisterAuthenticationFactor
							 	, _pCommandLine
							 	, _Params["UserID"].f_String()
							 	, _Params["Factor"].f_String()
							 	, _Params["Quiet"].f_Boolean()
							)
						;
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_UnregisterAuthenticationFactor
							 	, _pCommandLine
							 	, _Params["UserID"].f_String()
							 	, _Params["Factor"].f_String()
							)
						;
					}
				)
			;
			Distributed.f_RegisterCommand
				(
					{
						"Names"_= {"--trust-user-list-available-authentication-factor-types"}
						, "Category"_= Category
						, "Description"_= "List available authentication factors.\n"
						, "Options"_=
						{
							CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_EnumAuthenticationFactors, _pCommandLine, _Params["TableType"].f_String());
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
						, "Options"_=
						{
							CTableRenderHelper::fs_OutputTypeOption()
						}
					}
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self
							(
							 	&CDistributedAppActor::f_CommandLine_EnumUserAuthenticationFactors
							 	, _pCommandLine
							 	, _Params["UserID"].f_String()
							 	, _Params["TableType"].f_String()
							)
						;
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
					, [this](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
					{
						return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AuthenticatePermissionPattern, _pCommandLine, _Params);
					}
					, CDistributedAppCommandLineSpecification::ECommandFlag_WaitForRemotes
				)
			;
		}
	}
}
