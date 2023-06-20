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
	using namespace NContainer;
	using namespace NCommandLine;

	void CDistributedAppActor::fp_BuildDefaultCommandLine_Logging(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		o_CommandLine.f_RegisterGlobalOptions
			(
				{
					"ConcurrentLogging?"_o=
					{
						"Names"_o= {"--log-on-thread"}
						, "Default"_o= true
						, "Description"_o= "Log on a separate thread to minimally affect application performance and behavior."
					}
					, "StdErrLogger?"_o=
					{
						"Names"_o= {"--log-to-stderr"}
						, "Default"_o= false
						, "Description"_o= "Log to stderr."
					}
					, "LogSeverities?"_o=
					{
						"Names"_o= {"--log-severities"}
						, "Type"_o= NEncoding::CEJSONOrdered{COneOf("Critical", "Error", "Warning", "Info", "Debug", "DebugV1", "DebugV2", "DebugV3", "PInfo", "PWarning", "PError")}
						, "Description"_o= "Which log severities to log to stderr"
#if DMibEnableTrace > 0
						" or with trace logger."
#else
						"."
#endif
					}
				}
			)
		;

#if DMibEnableTrace > 0
		o_CommandLine.f_RegisterGlobalOptions
			(
				{
					"TraceLogger?"_o=
					{
						"Names"_o= {"--log-to-trace"}
						, "Default"_o= bool(NSys::fg_System_BeingDebugged())
						, "Description"_o= "Log to trace."
					}
				}
			)
		;
#endif
	}
}
