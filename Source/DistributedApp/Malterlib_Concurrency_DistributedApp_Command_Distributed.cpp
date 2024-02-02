// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NContainer;
	using namespace NCommandLine;
	using namespace NEncoding;
	using namespace NStorage;
	using namespace NStr;

	void CDistributedAppActor::fp_BuildDefaultCommandLine_DistributedComputing(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		auto Distributed = o_CommandLine.f_AddSection("Distributed Computing", "Use these commands to manage connectability and trust between different hosts.");

		auto IncludeFriendlyNameOption = "IncludeFriendlyName?"_o=
			{
				"Names"_o= {"--include-friendly-name"}
				, "Default"_o= true
				, "Description"_o= "Include friendly host name in output."
			}
		;

		auto ConnectionConcurrencyOption = "ConnectionConcurrency?"_o=
			{
				"Names"_o= {"--connection-concurrency"}
				, "Default"_o= -1
				, "Description"_o= "The number of parallel connections to connect to address with. Useful for increasing network throughput. Set to -1 to use default."
			}
		;

		auto PermissionUserAuthenticationFactorsOption = "AuthenticationFactors?"_o=
			{
				"Names"_o= {"--authentication-factors"}
				, "Type"_o= COneOfType{CEJSONOrdered({CEJSONOrdered({""})}), CEJSONOrdered({""}), CEJSONOrdered{""}}
				, "Description"_o= "The authentication factor(s) used for the permission. Factors must be specified as a string or a JSON array (of arrays):\n"
					"\"Factor1\"                                        - must authenticate by Factor1.\n"
					"[\"Factor1\", \"Factor2\"]                           - must authenticate by Factor1 or Factor2.\n"
					"[[\"Factor1\", \"Factor2\"]]                         - must authenticate by Factor1 and Factor2.\n"
					"[[\"Factor1\", \"Factor2\"], [\"Factor1\", \"Factor3\"]] - must authenticate by Factor1 and either Factor2 or Factor3.\n"
			}
		;

		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-host-id"}
					, "Description"_o= "Get the host ID for this application.\n"
					, "Output"_o= "The host ID of this application."
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetHostID, _pCommandLine);
				}
			)
		;
		Distributed.f_RegisterDirectCommand
			(
				{
					"Names"_o= {"--trust-remove-all-trust"}
					, "Description"_o= "Removes all certificates, clients, client connections and listen configs.\n"
					, "Options"_o=
					{
						"Confirm?"_o=
						{
							"Names"_o= {"--yes"}
							, "Default"_o= false
							, "Description"_o= "Confirm that you really want to remove all trust."
						}
					}
				}
				, [this](CEJSONSorted const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)
				{
					return f_CommandLine_RemoveAllTrust(_Parameters["Confirm"].f_Boolean());
				}
			)
		;

		CStr Category = "Outgoing connections";
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-list"}
					, "Category"_o= Category
					, "Description"_o= "List the outgoing addresses that the application tries to connect to.\n"
					, "Output"_o= "A new line separated list of outgoing connections."
					, "Options"_o=
					{
						IncludeFriendlyNameOption
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-connection-status"}
					, "Category"_o= Category
					, "Description"_o= "List the status for all outgoing connections.\n"
					, "Output"_o= "A table with the status for outgoing connections."
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}

				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetConnetionStatus, _pCommandLine, _Params["TableType"].f_String());
				}
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-debug-stats"}
					, "Category"_o= Category
					, "Description"_o= "Show debug info for connections.\n"
					, "Output"_o= "A table with debug info for connected hosts."
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}

				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetDebugStats, _pCommandLine, _Params["TableType"].f_String());
				}
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-add"}
					, "Category"_o= Category
					, "Description"_o= "Add a connection to a remote distributed actor host.\n"
						"The ticket encodes the url of the remote host and can usually be generated by the remote host by running --generate-connection-ticket.\n"
						"A ticket can only be used once and expires after a set time.\n"
					, "Output"_o= "The host information for the remote server."
					, "Parameters"_o=
					{
						"Ticket?"_o=
						{
							"Default"_o= ""
							, "Description"_o= "Specify ticket to use for adding the connection, if this is empty, the ticket will be promted for."
						}
					}
					, "Options"_o=
					{
						ConnectionConcurrencyOption
						, IncludeFriendlyNameOption
						, "TrustedNamespaces?"_o=
						{
							"Names"_o= {"--trusted-namespaces"}
							, "Type"_o= {""}
							, "Default"_o= _[_]
							, "Description"_o= "The namespaces that should be trusted for the host you are establishing trust with."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Add trust connection"
							, [=, this]() -> TCFuture<uint32>
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-set-concurrency"}
					, "Category"_o= Category
					, "Description"_o= "Sets the number of parallel connections to connect to address with. Useful for increasing network throughput. Set to -1 to use default.\n"
					, "Parameters"_o=
					{
						"ConnectionConcurrency"_o=
						{
							"Type"_o= -1
							, "Description"_o= "Specify ticket to use for adding the connection."
						}
					}
					, "Options"_o=
					{
						"Address"_o=
						{
							"Names"_o= {"--address"}
							, "Type"_o= ""
							, "Description"_o= "The client connection address to set connection concurrency for."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Set connection concurrency"
							, [=, this]() -> TCFuture<uint32>
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-add-additional"}
					, "Category"_o= Category
					, "Description"_o= "Add a connection to an already trusted host.\n"
					, "Output"_o= "The host information for the remote server."
					, "Parameters"_o=
					{
						"ConnectionURL"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The URL of the connection you want to add."
						}
					}
					, "Options"_o=
					{
						ConnectionConcurrencyOption
						, IncludeFriendlyNameOption
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Add additional trust connection"
							, [=, this]() -> TCFuture<uint32>
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-connection-remove"}
					, "Category"_o= Category
					, "Description"_o= "Remove an outgoing connection.\n"
					, "Parameters"_o=
					{
						"ConnectionURL"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The URL of the connection you want to remove."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Remove trust connection"
							, [=, this]() -> TCFuture<uint32>
							{
								return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveConnection, _pCommandLine, _Params["ConnectionURL"].f_String());
							}
						)
					;
				}
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Category = "Incoming connections trust";
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-trusted-list"}
					, "Category"_o= Category
					, "Description"_o= "List the incoming hosts that are trusted by this application.\n"
					, "Output"_o= "A line separated list of trusted connections."
					, "Options"_o=
					{
						IncludeFriendlyNameOption
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-trusted-remove"}
					, "Category"_o= Category
					, "Description"_o= "Remove the incoming trust for a host id.\n"
					, "Parameters"_o=
					{
						"HostID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The host ID of the host you want to remove trust for."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Remove trusted host"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-generate-ticket"}
					, "Category"_o= Category
					, "Description"_o= "Generate a ticket that can be used to connect to this application from remote host.\n"
						" The ticket encodes the url of this host."
						" A ticket can only be used once and expires after a set time."
						" Once used with --trust-add-connection on the remote host the remote host will be trusted by this application."
					, "Output"_o= "The trust ticket that can be used with --trust-add-connection on the remote application."
					, "Options"_o=
					{
						"Permissions?"_o=
						{
							"Names"_o= {"--permissions"}
							, "Type"_o= {""}
							, "Default"_o= _[_]
							, "Description"_o= "The permissions that should be granted to the host/user that uses the returned ticket."
						}
						, "UserID?"_o=
						{
							"Names"_o= {"--user"}
							, "Type"_o= ""
							, "Description"_o= "If specified, the permissions are added for the specific user for the host that uses the ticket."
						}
						, PermissionUserAuthenticationFactorsOption
					}
					, "Parameters"_o=
					{
						"ForListenURL?"_o=
						{
							"Default"_o= ""
							, "Description"_o=  "Specify the URL you want to generate a ticket for.\n"
								"This is needed because the ticket includes the public certificate to connect to this server. Every host has a different certificate"
								" so you need to specify the listen address to be able to know which certificate to send with the ticket.\n"
								"If you do not specify the URL the primary listen URL will be used. The primary listen URL is the first listen URL specified in the config file."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Generate trust ticket"
							, [=, this]() -> TCFuture<uint32>
							{
								TCSet<CStr> Permissions;
								for (auto &PermissionJSON : _Params["Permissions"].f_Array())
									Permissions[PermissionJSON.f_String()];

								CStr UserID;
								if (auto *pValue =_Params.f_GetMember("UserID"))
									UserID = pValue->f_String();

								CEJSONSorted AuthenticationFactors;
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
					"Names"_o= {"--trust-listen-list"}
					, "Category"_o= Category
					, "Description"_o= "List addresses that this application listens for incoming connections on.\n"
					, "Output"_o= "A new line separated list of listen addresses."
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ListListen, _pCommandLine, _Params["TableType"].f_String());
				}
			)
		;

		CStr OverwriteWarning = fg_Format("WARNING: This will overwrite the '{}' config file and strip any comments.\n", mp_State.m_ConfigDatabase.f_GetFileName());

		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-listen-add"}
					, "Category"_o= Category
					, "Description"_o= "Start listening for connections on address.\n"
						+ OverwriteWarning
					, "Parameters"_o=
					{
						"ListenURL"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The address you want to start listening on."
						}
						, "MakePrimary?"_o=
						{
							"Default"_o= false
							, "Description"_o= ""
						}
					}
					, "Options"_o=
					{
						"Primary?"_o=
						{
							"Names"_o= {"--primary", "-p"}
							, "Default"_o= false
							, "Description"_o= "Make the new address the primary listen.\n"
							"If no listen currently exists the new address will always be made primary."
						}
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Add trust listen address"
							, [=, this]() -> TCFuture<uint32>
							{
								return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AddListen, _pCommandLine, _Params["ListenURL"].f_String(), _Params["Primary"].f_Boolean());
							}
						)
					;
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-listen-remove"}
					, "Category"_o= Category
					, "Description"_o= "Stop listening for connections on address.\n"
						+ OverwriteWarning
					, "Parameters"_o=
					{
						"ListenURL"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The address you want to stop listening on."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Remove listen address"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-listen-set-primary"}
					, "Category"_o= Category
					, "Description"_o= "Set the primary listen URL.\n"
						"This URL is the URL that will be used by default when --trust-generate-ticket is called without specifying the URL.\n"
						+ OverwriteWarning
					, "Parameters"_o=
					{
						"ListenURL"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The address you want to use as the primary listen address."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Set primary listen address"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-namespace-list"}
					, "Category"_o= Category
					, "Description"_o= "List actor publication namespaces that this application publishes as well as the hosts that are trusted for those namespaces.\n"
					, "Output"_o= "A new line separated list of namespaces and the hosts that are trusted and untrusted."
					, "Options"_o=
					{
						"IncludeTrustedHosts?"_o=
						{
							"Names"_o= {"--include-hosts"}
							, "Default"_o= true
							, "Description"_o= "Include trusted and untrusted hosts in output."
						}
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-namespace-add-trusted-host"}
					, "Category"_o= Category
					, "Description"_o= "Trust a host for the specified namespace.\n"
					, "Parameters"_o=
					{
						"TrustHost"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The host to trust."
						}
					}
					, "Options"_o=
					{
						"TrustNamespace"_o=
						{
							"Names"_o= {"--namespace"}
							, "Type"_o= ""
							, "Description"_o= "The namespace to trust the host for."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Trusted host for namespace"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-namespace-remove-trusted-host"}
					, "Category"_o= Category
					, "Description"_o= "Untrust a host for the specified namespace.\n"
					, "Parameters"_o=
					{
						"TrustHost"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The host to untrust."
						}
					}
					, "Options"_o=
					{
						"TrustNamespace"_o=
						{
							"Names"_o= {"--namespace"}
							, "Type"_o= ""
							, "Description"_o= "The namespace to untrust the host for."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Untrust host for namespace"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-permission-list"}
					, "Category"_o= Category
					, "Description"_o= "List host permissions.\n"
					, "Output"_o= "A new line separated list of permissions and the hosts that have those permissions."
					, "Options"_o=
					{
						"IncludeTargets?"_o=
						{
							"Names"_o= {"--include-targets"}
							, "Default"_o= true
							, "Description"_o= "Include the targets (hosts/users) that have the permissions in the output."
						}
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-permission-add"}
					, "Category"_o= Category
					, "Description"_o= "Add a permission to a host, a user, or a host/user pair.\n"
					, "Parameters"_o=
					{
						"Permission"_o=
						{
							"Type"_o= COneOfType(CEJSONOrdered{""}, "")
							, "Description"_o= "The permission to add."
						}
					}
					, "Options"_o=
					{
						"HostID?"_o=
						{
							"Names"_o= {"--host"}
							, "Type"_o= ""
							, "Description"_o= "The host to add a permission to."
						}
						, "UserID?"_o=
						{
							"Names"_o= {"--user"}
							, "Type"_o= ""
							, "Description"_o= "The user to add the permission to."
						}
						, "MaxLifetime?"_o=
						{
							"Names"_o= {"--max-lifetime"}
							,"Default"_o= CPermissionRequirements::mc_DefaultMaximumLifetime
							, "Description"_o= "The maximum number of minutes that a cached authentication can be used for this granted permission."
						}
						, PermissionUserAuthenticationFactorsOption
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Add permissions"
							, [=, this]() -> TCFuture<uint32>
							{
								CStr HostID;
								if (auto *pValue =_Params.f_GetMember("HostID"))
									HostID = pValue->f_String();
								CStr UserID;
								if (auto *pValue =_Params.f_GetMember("UserID"))
									UserID = pValue->f_String();
								CEJSONSorted AuthenticationFactors;
								if (auto *pValue =_Params.f_GetMember("AuthenticationFactors"))
									AuthenticationFactors = pValue->f_Array();

								TCVector<CStr> Permissions;
								if (_Params["Permission"].f_IsString())
									Permissions.f_Insert(_Params["Permission"].f_String());
								else
									Permissions = _Params["Permission"].f_StringArray();

								int64 Lifetime = _Params["MaxLifetime"].f_Integer();
								return g_Future <<= self
									(
										&CDistributedAppActor::f_CommandLine_AddPermission
										, _pCommandLine
										, HostID
										, UserID
										, Permissions
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
					"Names"_o= {"--trust-permission-remove"}
					, "Category"_o= Category
					, "Description"_o= "Remove a permission from a host, a user, or a host/user pair.\n"
					, "Parameters"_o=
					{
						"Permission"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The permission to remove."
						}
					}
					, "Options"_o=
					{
						"HostID?"_o=
						{
							"Names"_o= {"--host"}
							, "Type"_o= ""
							, "Description"_o= "The host to remove the permission from."
						}
						, "UserID?"_o=
						{
							"Names"_o= {"--user"}
							, "Type"_o= ""
							, "Description"_o= "The user to remove the permission from."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return fp_RunCommandLineAndLogError
						(
							"Remove permissions"
							, [=, this]() -> TCFuture<uint32>
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
					"Names"_o= {"--trust-user-add"}
					, "Category"_o= Category
					, "Description"_o= "Add a user to the database.\n"
					, "Output"_o= "None."
					, "Options"_o=
					{
						"Quiet?"_o=
						{
							"Names"_o= {"--quiet"}
							, "Default"_o= false
							, "Description"_o= "Don't output user ID."
						}
					}
					, "Parameters"_o=
					{
						"UserName"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The name of the user being added to the database."
						}
					}
			}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AddUser, _pCommandLine, _Params);
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-remove"}
					, "Category"_o= Category
					, "Description"_o= "Remove a user from the database.\n"
					, "Output"_o= "None."
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to remove from the database."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_RemoveUser, _pCommandLine, _Params["UserID"].f_String());
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-list"}
					, "Category"_o= Category
					, "Description"_o= "List the users in the database.\n"
					, "Output"_o= "A newline separated list of the users."
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ListUsers, _pCommandLine, _Params["TableType"].f_String());
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-set-userinfo"}
					, "Category"_o= Category
					, "Description"_o= "Set or add metadata to a user.\n"
					"Change user name or add new key, value pairs to the user's metadata or replace a value if the key already exists."
					, "Options"_o=
					{
						"Metadata?"_o=
						{
							"Names"_o= {"--metadata"}
							, "Type"_o= EJSONType_Object
							, "Description"_o= "The metadata to set.\n"
							"The metadata is specified as a JSON object '{\"Key\" : \"Value\"}'"
						}
						, "UserName?"_o=
						{
							"Names"_o= {"--username"}
							, "Type"_o= ""
							, "Description"_o= "The new username."
						}
					}
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to change the information for."
						}
					}

				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_SetUserInfo, _pCommandLine, _Params);
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-remove-metadata"}
					, "Category"_o= Category
					, "Description"_o= "Remove the metadata matching key from the user."
					, "Options"_o=
					{

						"Key"_o=
						{
							"Names"_o= {"--key"}
							, "Type"_o= ""
							, "Description"_o= "Key of the metadata to remove."
						}
					}
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to remove metadata from."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-user-get-default-user"}
					, "Category"_o= Category
					, "Description"_o= "Get the default user name.\n"
					, "Output"_o= "The default username."
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_GetDefaultUser, _pCommandLine);
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-set-default-user"}
					, "Category"_o= Category
					, "Description"_o= "Set the user name to use.\n"
					, "Output"_o= "None."
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to use."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_SetDefaultUser, _pCommandLine, _Params["UserID"].f_String());
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-export"}
					, "Category"_o= Category
					, "Description"_o= "Export a user from the database.\n"
					, "Output"_o= "An encoded text representation of the user data."
					, "Options"_o=
					{

						"IncludePrivate?"_o=
						{
							"Names"_o= {"--include-private-data"}
							, "Default"_o= false
							, "Description"_o= "In addition to data needed to verify a users identity, include private data needed to authenticate as that user."
						}
					}
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID of the user to export."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-user-import"}
					, "Category"_o= Category
					, "Description"_o= "Import a user into the database.\n"
					, "Output"_o= "None."
					, "Parameters"_o=
					{
						"UserData"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The encoded text representation of the user data."
						}
					}
			}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_ImportUser, _pCommandLine, _Params["UserData"].f_String());
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-add-authentication-factor"}
					, "Category"_o= Category
					, "Description"_o= "Add an authentication factor to the user."
					, "Options"_o=
					{
						"Factor"_o=
						{
							"Names"_o= {"--authentication-factor"}
							, "Type"_o= ""
							, "Description"_o= "The authentication factor to add."
						}
						, "Quiet?"_o=
						{
							"Names"_o= {"--quiet"}
							, "Default"_o= false
							, "Description"_o= "Don't output factor ID."
						}
					}
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to add an authentication factor to."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-user-remove-authentication-factor"}
					, "Category"_o= Category
					, "Description"_o= "Remove an authentication factor to the user."
					, "Options"_o=
					{
						"Factor"_o=
						{
							"Names"_o= {"--authentication-factor"}
							, "Type"_o= ""
							, "Description"_o= "The authentication factor to remove."
						}
					}
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "The user ID to remove an authentication factor from."
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-user-list-available-authentication-factor-types"}
					, "Category"_o= Category
					, "Description"_o= "List available authentication factors.\n"
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_EnumAuthenticationFactors, _pCommandLine, _Params["TableType"].f_String());
				}
			)
		;
		Distributed.f_RegisterCommand
			(
				{
					"Names"_o= {"--trust-user-list-authentication-factors"}
					, "Category"_o= Category
					, "Description"_o= "List the authentication factors registered to a user.\n"
					, "Parameters"_o=
					{
						"UserID"_o=
						{
							"Type"_o= ""
							, "Description"_o= "List the authentication factors registered for the user."
						}
					}
					, "Options"_o=
					{
						CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
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
					"Names"_o= {"--trust-user-authenticate-pattern"}
					, "Category"_o= Category
					, "Description"_o= "Give advance authentication for all permissions that matches the pattern.\n"
					, "Options"_o=
					{
						"AuthenticationFactors"_o=
						{
							"Names"_o= {"--authentication-factors"}
							, "Type"_o= COneOfType{CEJSONOrdered({""}), CEJSONOrdered{""}}
							, "Description"_o= "The factor(s) used for authentication.\n"
							"Authenticate these factors in advance.\n"
						}
						, "JSONOutput?"_o=
						{
							"Names"_o= {"--json-output"}
							, "Default"_o= false
							, "Description"_o= "Return authentication failures/successes in EJSON format."
						}
					}
					, "Parameters"_o=
					{
						"Pattern"_o=
						{
							"Type"_o= ""
							, "Description"_o= "Pattern for matching permissions.\n"
							"All permissions matching this pattern will preauthenticated with the listed factors.\n"
						}
					}
				}
				, [this](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine)
				{
					return g_Future <<= self(&CDistributedAppActor::f_CommandLine_AuthenticatePermissionPattern, _pCommandLine, _Params);
				}
				, EDistributedAppCommandFlag_WaitForRemotes
			)
		;
	}

	void CDistributedAppActor::fp_BuildDefaultCommandLine_DistributedComputingAuthentication(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		o_CommandLine.f_RegisterGlobalOptions
			(
				{
					"AuthenticationLifetime?"_o=
					{
						"Names"_o= {"--authentication-lifetime"}
						,"Default"_o= CPermissionRequirements::mc_OverrideLifetimeNotSet
						, "Description"_o= "The number of minutes until the authentication expires when cached."
						, "ValidForDirectCommand"_o= false
					}
				}
			)
		;
		o_CommandLine.f_RegisterGlobalOptions
			(
				{
					"AuthenticationUser?"_o=
					{
						"Names"_o= {"--authentication-user"}
						,"Type"_o= ""
						, "Description"_o= "Override the default user ID used for authentication."
						, "ValidForDirectCommand"_o= false
					}
				}
			)
		;
	}
}
