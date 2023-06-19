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
						, "Description"_= "Log to stderr."
					}
					, "LogSeverities?"_=
					{
						"Names"_= {"--log-severities"}
						, "Type"_= NEncoding::CEJSONSorted{COneOf("Critical", "Error", "Warning", "Info", "Debug", "DebugV1", "DebugV2", "DebugV3", "PInfo", "PWarning", "PError")}
						, "Description"_= "Which log severities to log to stderr"
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
}
