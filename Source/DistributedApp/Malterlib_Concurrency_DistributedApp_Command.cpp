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
		if (_Functionalities & EDefaultCommandLineFunctionality_Terminal)
			o_CommandLine.f_AddTerminalOptions();

		if (_Functionalities & EDefaultCommandLineFunctionality_Help)
			o_CommandLine.f_AddHelpCommand();

		if (_Functionalities & EDefaultCommandLineFunctionality_Logging)
			fp_BuildDefaultCommandLine_Logging(o_CommandLine);

		if (_Functionalities & EDefaultCommandLineFunctionality_Sensor)
			fp_BuildDefaultCommandLine_Sensor(o_CommandLine);

		if (_Functionalities & EDefaultCommandLineFunctionality_Authentication)
			fp_BuildDefaultCommandLine_DistributedComputingAuthentication(o_CommandLine);

		if (_Functionalities & EDefaultCommandLineFunctionality_DistributedComputing)
			fp_BuildDefaultCommandLine_DistributedComputing(o_CommandLine);
	}
}
