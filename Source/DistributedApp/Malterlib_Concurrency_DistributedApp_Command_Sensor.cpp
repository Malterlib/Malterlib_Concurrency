// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/Concurrency/DistributedAppSensorStoreLocal>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCommandLine;
	using namespace NEncoding;
	using namespace NTime;

	void CDistributedAppActor::fp_BuildDefaultCommandLine_Sensor(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		fp_BuildDefaultCommandLine_Sensor_Customizable
			(
				o_CommandLine.f_AddSection("Sensor", "Use these commands to manage the local store of sensor logs.")
				, ""
				, g_ActorFunctor / [this]
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_SensorList, _pCommandLine, _Filter, _Flags, _TableType);
				}
				, g_ActorFunctor / [this]
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorStatusFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_SensorStatus, _pCommandLine, _Filter, _Flags, _TableType);
				}
				, g_ActorFunctor / [this]
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
					, uint64 _MaxEntries
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_SensorReadingsList, _pCommandLine, _Filter, _MaxEntries, _Flags, _TableType);
				}
				, EDistributedAppCommandFlag_None
			)
		;
	}

	void CDistributedAppActor::fp_BuildDefaultCommandLine_Sensor_Customizable
		(
			CDistributedAppCommandLineSpecification::CSection _Section
			, CStr const &_Prefix
			, TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
			> &&_fSensorList
			, TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorStatusFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
			> &&_fSensorStatus
			,
			TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSONSorted const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
					, uint64 _MaxEntries
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
			> && _fSensorReadingsList
			, EDistributedAppCommandFlag _CommandFlags
		)
	{
		auto Option_SensorHostID = "HostID?"_o=
			{
				"Names"_o= {"--host-id"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only sensors from specified host id."
			}
		;

		auto Option_SensorApplication = "Application?"_o=
			{
				"Names"_o= {"--application"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only sensors that come from the specified application. Supports wildcards.\n"
				"Set to an empty string to only include sensors that are in host scope."
			}
		;

		auto Option_SensorIdentifier = "Identifier?"_o=
			{
				"Names"_o= {"--identifier"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only sensors that match the identifier. Supports wildcards."
			}
		;

		auto Option_SensorIdentifierScope = "IdentifierScope?"_o=
			{
				"Names"_o= {"--identifier-scope"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only sensors that match the idendifier scope. Supports wildcards."
			}
		;

		auto Option_Verbose = "Verbose?"_o=
			{
				"Names"_o= {"--verbose", "-v"}
				, "Default"_o= false
				, "Description"_o= "Output all properties."
			}
		;

		auto Option_Json = "Json?"_o=
			{
				"Names"_o= {"--json", "-j"}
				, "Default"_o= false
				, "Description"_o= "Output in json format."
			}
		;

		auto fOption_OnlyProblems = [](bool _bDefault)
			{
				return "OnlyProblems?"_o=
					{
						"Names"_o= {"--only-problems"}
						, "Default"_o= _bDefault
						, "Description"_o= "Only report problems."
					}
				;
			}
		;

		auto fOption_IgnoreRemoved = [](bool _bDefault)
			{
				return "IgnoreRemoved?"_o=
					{
						"Names"_o= {"--ignore-removed"}
						, "Default"_o= _bDefault
						, "Description"_o= "Ignore sensors that were removed, or that were owned by a removed application or app manager."
					}
				;
			}
		;

		auto fParseSensorFilter = [](CEJSONSorted const &_Params) -> CDistributedAppSensorReader_SensorFilter
			{
				CDistributedAppSensorReader_SensorFilter Filter;

				if (auto pValue = _Params.f_GetMember("HostID"))
					Filter.m_HostID = pValue->f_String();

				if (auto pValue = _Params.f_GetMember("Application"))
				{
					auto Application = pValue->f_String();
					if (Application.f_IsEmpty())
						Filter.m_Scope = CDistributedAppSensorReporter::CSensorScope_Host{};
					else
						Filter.m_Scope = CDistributedAppSensorReporter::CSensorScope_Application{pValue->f_String()};
				}

				if (auto pValue = _Params.f_GetMember("Identifier"))
					Filter.m_Identifier = pValue->f_String();

				if (auto pValue = _Params.f_GetMember("IdentifierScope"))
					Filter.m_IdentifierScope = pValue->f_String();
					
				if (auto pValue = _Params.f_GetMember("IgnoreRemoved"); pValue->f_Boolean())
					Filter.m_Flags |= CDistributedAppSensorReader_SensorFilter::ESensorFlag_IgnoreRemoved;

				return Filter;
			}
		;

		auto fParseFlags = [](CEJSONSorted const &_Params)
			{
				ESensorOutputFlag Flags = ESensorOutputFlag_None;
				if (_Params["Verbose"].f_Boolean())
					Flags |= ESensorOutputFlag_Verbose;
				if (_Params["Json"].f_Boolean())
					Flags |= ESensorOutputFlag_Json;

				return Flags;
			}
		;

		_Section.f_RegisterCommand
			(
				{
					"Names"_o= {"--{}sensor-list"_f << _Prefix}
					, "Description"_o= "List the sensors that are in the local store.\n"
					, "Output"_o= "A table of sensors."
					, "Options"_o=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_SensorIdentifierScope
						, fOption_IgnoreRemoved(false)
						, Option_Verbose
						, Option_Json
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorList = fg_Move(_fSensorList), fParseSensorFilter, fParseFlags](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					co_return co_await fSensorList
						(
							_Params
							, _pCommandLine
							, fParseSensorFilter(_Params)
							, fParseFlags(_Params)
							, _Params["TableType"].f_String()
						)
					;
				}
				, _CommandFlags
			)
		;

		_Section.f_RegisterCommand
			(
				{
					"Names"_o= {"--{}sensor-status"_f << _Prefix}
					, "Description"_o= "List the current value for sensors that are in the local store.\n"
					, "Output"_o= "A table of sensors readings."
					, "Options"_o=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_SensorIdentifierScope
						, fOption_IgnoreRemoved(true)
						, Option_Verbose
						, Option_Json
						, fOption_OnlyProblems(true)
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorStatus = fg_Move(_fSensorStatus), fParseSensorFilter, fParseFlags]
				(CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					CDistributedAppSensorReader_SensorStatusFilter Filter;
					Filter.m_SensorFilter = fParseSensorFilter(_Params);

					if (_Params["OnlyProblems"].f_Boolean())
						Filter.m_Flags |= CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems;

					co_return co_await fSensorStatus
						(
							_Params
							, _pCommandLine
							, Filter
							, fParseFlags(_Params)
							, _Params["TableType"].f_String()
						)
					;
				}
				, _CommandFlags
			)
		;

		_Section.f_RegisterCommand
			(
				{
					"Names"_o= {"--{}sensor-readings-list"_f << _Prefix}
					, "Description"_o= "List historic values for sensors that are in the local store.\n"
					, "Output"_o= "A table of sensors readings."
					, "Options"_o=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_SensorIdentifierScope
						, fOption_IgnoreRemoved(false)
						, Option_Verbose
						, Option_Json
						, "MinTimestamp?"_o=
						{
							"Names"_o= {"--min-timestamp"}
							, "Type"_o= CTime()
							, "Description"_o= "Limit output to readings that are newer than specified."
						}
						, "MinSequence?"_o=
						{
							"Names"_o= {"--min-sequence"}
							, "Type"_o= 0
							, "Description"_o= "Limit output to sequences that are greater than specified for the timestamp."
						}
						, "MaxTimestamp?"_o=
						{
							"Names"_o= {"--max-timestamp"}
							, "Type"_o= CTime()
							, "Description"_o= "Limit output to readings that are older than specified."
						}
						, "MaxSequence?"_o=
						{
							"Names"_o= {"--max-sequence"}
							, "Type"_o= 0
							, "Description"_o= "Limit output to sequences that are less than specified for the timestamp."
						}
						, "ReportNewestFirst?"_o=
						{
							"Names"_o= {"--newest"}
							, "Default"_o= true
							, "Description"_o= "When applying entry limit, show newest entries first."
						}
						, fOption_OnlyProblems(false)
						, "MaxEntries?"_o=
						{
							"Names"_o= {"--max-entries"}
							, "Default"_o= 100
							, "Description"_o= "Limit the number of output lines.\n"
							"Set to 0 for unlimited entries"
						}
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorReadingsList = fg_Move(_fSensorReadingsList), fParseSensorFilter, fParseFlags]
				(CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					CDistributedAppSensorReader_SensorReadingFilter Filter;
					Filter.m_SensorFilter = fParseSensorFilter(_Params);

					if (auto pValue = _Params.f_GetMember("MinSequence"))
						Filter.m_MinSequence = pValue->f_Integer();

					if (auto pValue = _Params.f_GetMember("MaxSequence"))
						Filter.m_MaxSequence = pValue->f_Integer();

					if (auto pValue = _Params.f_GetMember("MinTimestamp"))
						Filter.m_MinTimestamp = pValue->f_Date().f_ToUTC();

					if (auto pValue = _Params.f_GetMember("MaxTimestamp"))
						Filter.m_MaxTimestamp = pValue->f_Date().f_ToUTC();

					Filter.m_Flags = CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_None;
					if (_Params["ReportNewestFirst"].f_Boolean())
						Filter.m_Flags |= CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst;
					if (_Params["OnlyProblems"].f_Boolean())
						Filter.m_Flags |= CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_OnlyProblems;

					co_return co_await fSensorReadingsList
						(
							_Params
							, _pCommandLine
							, Filter
							, _Params["MaxEntries"].f_Integer()
							, fParseFlags(_Params)
							, _Params["TableType"].f_String()
						)
					;
				}
				, _CommandFlags
			)
		;
	}

	namespace
	{
		auto constexpr gc_fSensorDataToJson = [](auto const &_Value) -> CEJSONSorted
			{
				using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
				if constexpr (NTraits::TCIsSame<CValueType, CVoidTag>::mc_Value)
					return nullptr;
				else if constexpr (NTraits::TCIsSame<CValueType, CDistributedAppSensorReporter::CStatus>::mc_Value)
				{
					return CEJSONUserTypeSorted
						(
							"Status"
							,
							{
								"Severity"_j= _Value.m_Severity
								, "Description"_j= _Value.m_Description
							}
						)
					;
				}
				else if constexpr (NTraits::TCIsSame<CValueType, CDistributedAppSensorReporter::CVersion>::mc_Value)
				{
					return CEJSONUserTypeSorted
						(
							"Version"
							,
							{
								"Identifier"_j= _Value.m_Identifier
								, "Major"_j= _Value.m_Major
								, "Minor"_j= _Value.m_Minor
								, "Revision"_j= _Value.m_Revision
							}
						)
					;
				}
				else
					return _Value;
			}
		;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorListOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
		)
	{
		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		CTableRenderHelper::CColumnHelper Columns((_Flags & ESensorOutputFlag_Verbose) ? 1 : 0);

		Columns.f_AddHeading("Host ID", 0);
		Columns.f_AddHeading("Host Description", 0);
		Columns.f_AddHeading("Application", 0);
		Columns.f_AddHeading("Identifier", 0);
		Columns.f_AddHeading("Identifier Scope", 0);
		Columns.f_AddHeading("Name", 0);
		Columns.f_AddHeading("Type", 1);
		Columns.f_AddHeading("Expected Report Interval", 1);
		Columns.f_AddHeading("Unit Divisors", 1);
		Columns.f_AddHeading("Warn Value", 1);
		Columns.f_AddHeading("Critical Value", 1);
		Columns.f_AddHeading("Removed", 1);
		Columns.f_AddHeading("Snoozed Until", 1);
		Columns.f_AddHeading("Paused", 1);

		TableRenderer.f_AddHeadings(&Columns);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;

		CEJSONSorted JsonOutput;
		auto &JsonOutputArray = JsonOutput.f_Array();

		for (auto iSensors = co_await fg_Move(_Sensors).f_GetIterator(); iSensors; co_await ++iSensors)
		{
			for (auto &SensorInfo : *iSensors)
			{
				CStr Application;
				if (SensorInfo.m_Scope.f_IsOfType<CDistributedAppSensorReporter::CSensorScope_Application>())
					Application = SensorInfo.m_Scope.f_GetAsType<CDistributedAppSensorReporter::CSensorScope_Application>().m_ApplicationName;

				if (_Flags & ESensorOutputFlag_Json)
				{
					CEJSONSorted UnitDivisors;
					auto &UnitDivisorsArray = UnitDivisors.f_Array();
					for (auto &Divisor : SensorInfo.m_UnitDivisors)
					{
						UnitDivisorsArray.f_Insert
							(
								CEJSONSorted
								{
									"Divisor"_= SensorInfo.m_UnitDivisors.fs_GetKey(Divisor).f_VisitRet<CEJSONSorted>(gc_fSensorDataToJson)
									, "nDecimals"_= Divisor.m_nDecimals
									, "UnitFormatter"_= Divisor.m_UnitFormatter
								}
							)
						;
					}

					auto fComparisonValue = [](NStorage::TCOptional<CDistributedAppSensorReporter::CValueComparison> const &_Value) -> CEJSONSorted
						{
							if (!_Value)
								return nullptr;

							auto &Value = *_Value;

							return
								{
									"CompareToValue"_= Value.m_CompareToValue.f_VisitRet<CEJSONSorted>(gc_fSensorDataToJson)
									, "Operator"_= Value.m_Operator
								}
							;
						}
					;

					JsonOutputArray.f_Insert
						(
							CEJSONSorted
							{
								"HostID"_= SensorInfo.m_HostID
								, "HostName"_= SensorInfo.m_HostName
								, "Application"_= Application
								, "Identifier"_= SensorInfo.m_Identifier
								, "IdentifierScope"_= SensorInfo.m_IdentifierScope
								, "Name"_= SensorInfo.m_Name
								, "Type"_= SensorInfo.m_Type
								, "ExpectedReportInterval"_= SensorInfo.m_ExpectedReportInterval
								, "UnitDivisors"_= fg_Move(UnitDivisors)
								, "WarnValue"_= fComparisonValue(SensorInfo.m_WarnValue)
								, "CriticalValue"_= fComparisonValue(SensorInfo.m_CriticalValue)
								, "Removed"_= SensorInfo.m_bRemoved
								, "SnoozeUntil"_= SensorInfo.m_SnoozeUntil
								, "PauseReportingFor"_= SensorInfo.m_PauseReportingFor
							}
						)
					;
				}
				else
				{
					bHasHostID = bHasHostID || SensorInfo.m_HostID;
					bHasHostName = bHasHostName || SensorInfo.m_HostName;
					bHasApplication = bHasApplication || Application;

					TableRenderer.f_AddRow
						(
							SensorInfo.m_HostID
							, CHostInfo::fs_FormatFriendlyNameForTable(SensorInfo.m_HostName, AnsiEncoding)
							, Application
							, SensorInfo.m_Identifier
							, SensorInfo.m_IdentifierScope
							, SensorInfo.m_Name
							, CDistributedAppSensorReporter::fs_SensorDataTypeToString(SensorInfo.m_Type)
							, SensorInfo.m_ExpectedReportInterval
							, CDistributedAppSensorReporter::fs_FormatSensorDivisors(SensorInfo.m_UnitDivisors, "\n")
							, SensorInfo.m_WarnValue
							, SensorInfo.m_CriticalValue
							, SensorInfo.m_bRemoved ? "true" : "false"
							, SensorInfo.m_SnoozeUntil.f_IsValid() ? CStr("{tc6}"_f << SensorInfo.m_SnoozeUntil) : CStr()
							, fg_SecondsDurationToHumanReadable(SensorInfo.m_PauseReportingFor)
						)
					;
				}
			}
		}

		if (_Flags & ESensorOutputFlag_Json)
			*_pCommandLine += JsonOutput.f_ToString("\t", EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat);
		else
		{
			if (!bHasHostID)
				Columns.f_SetVerbose("Host ID");
			if (!bHasHostName)
				Columns.f_SetVerbose("Host Description");
			if (!bHasApplication)
				Columns.f_SetVerbose("Application");

			TableRenderer.f_Output(_TableType);
		}

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorList
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppSensorReader_SensorFilter const &_Filter
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenSensorStoreLocal();

		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, CDistributedAppSensorReader::CGetSensors{.m_Filters = {_Filter}});

		co_return co_await self
			(
				&CDistributedAppActor::f_CommandLine_SensorListOutput
				, _pCommandLine
				, fg_Move(SensorsGenerator)
				, _Flags
				, _TableType
			)
		;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorReadingsOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> &&_SensorReadings
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
			, uint64 _MaxEntries
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
			, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
		)
	{
		TCPromise<void> Cancelled;
		auto CancellationSubscription = _pCommandLine->f_RegisterForCancellation
			(
				g_ActorFunctor
				(
					g_ActorSubscription / []()
					{
					}
				)
				/ [Cancelled]() -> TCFuture<bool>
				{
					Cancelled.f_SetResult();
					co_return false;
				}
			)
		;

		auto [Result, bCancelled] = co_await fg_AnyDone
			(
				self
				(
					&CDistributedAppActor::fp_CommandLine_SensorReadingsOutput
					, _pCommandLine
					, fg_Move(_SensorReadings)
					, fg_Move(_Sensors)
					, _MaxEntries
					, _Flags
					, _TableType
					, _Filter
				)
				, Cancelled.f_Future()
			)
		;

		if (Result)
			co_return *Result;
		else
			co_return DMibErrorInstance("Cancelled");
	}

	TCFuture<uint32> CDistributedAppActor::fp_CommandLine_SensorReadingsOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReader_SensorKeyAndReading>> &&_SensorReadings
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppSensorReporter::CSensorInfo>> &&_Sensors
			, uint64 _MaxEntries
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
			, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
		)
	{
		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		CAnsiEncoding AnsiEncodingNoColor(AnsiEncoding.f_Flags() & ~(EAnsiEncodingFlag_Color | EAnsiEncodingFlag_Color24Bit | EAnsiEncodingFlag_ColorLightBackground));

		TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CDistributedAppSensorReporter::CSensorInfo> SensorInfos;
		{
			for (auto iSensors = co_await fg_Move(_Sensors).f_GetIterator(); iSensors; co_await ++iSensors)
			{
				for (auto &SensorInfo : *iSensors)
					SensorInfos[SensorInfo.f_Key()] = SensorInfo;
			}
		}

		CTableRenderHelper::CColumnHelper Columns((_Flags & ESensorOutputFlag_Verbose) ? 1 : 0);

		Columns.f_AddHeading("Host ID", 1);
		Columns.f_AddHeading("Host Description", 0);
		Columns.f_AddHeading("Application", 0);
		Columns.f_AddHeading("Identifier", 1);
		Columns.f_AddHeading("Identifier Scope", 1);
		Columns.f_AddHeading("Name", 0);
		Columns.f_AddHeading("Timestamp", 0);
		Columns.f_AddHeading("Sequence", 1);
		Columns.f_AddHeading("Value", 0);
		Columns.f_AddHeading("Snoozed Until", 0);
		Columns.f_AddHeading("Outdated", 0);

		TableRenderer.f_AddHeadings(&Columns);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;
		bool bHasOutdated = false;
		bool bHasSnoozeUntil = false;
		uint64 nEntries = 0;

		CEJSONSorted JsonOutput;
		auto &JsonOutputArray = JsonOutput.f_Array();

		CClock LastOutput{true};
		auto fOutputStatus = [&]
			{
				if (LastOutput.f_GetTime() > 0.016 || nEntries == _MaxEntries)
				{
					CUStr ToOutput = CStr("  {} entries"_f << nEntries);
					*_pCommandLine %= "{}\x1B[{}D"_f << ToOutput << ToOutput.f_GetLen();
					LastOutput.f_Start();
				}
			}
		;

		uint32 Return = 0;

		NTime::CTime Now = NTime::CTime::fs_NowUTC();

		for (auto iReadings = co_await fg_Move(_SensorReadings).f_GetIterator(); iReadings; co_await ++iReadings)
		{
			for (auto &Reading : *iReadings)
			{
				CStr Application;
				if (Reading.m_SensorInfoKey.m_Scope.f_IsOfType<CDistributedAppSensorReporter::CSensorScope_Application>())
					Application = Reading.m_SensorInfoKey.m_Scope.f_GetAsType<CDistributedAppSensorReporter::CSensorScope_Application>().m_ApplicationName;

				CStr HostName;
				CTime SnoozeUntil;
				auto *pSensorInfo = SensorInfos.f_FindEqual(Reading.m_SensorInfoKey);
				if (pSensorInfo)
				{
					HostName = pSensorInfo->m_HostName;
					SnoozeUntil = pSensorInfo->m_SnoozeUntil;
				}

				bHasHostID = bHasHostID || Reading.m_SensorInfoKey.m_HostID;
				bHasHostName = bHasHostName || HostName;
				bHasApplication = bHasApplication || Application;
				bHasSnoozeUntil = bHasSnoozeUntil || SnoozeUntil.f_IsValid();
				bool bIsSnoozed = SnoozeUntil.f_IsValid() && Now <= SnoozeUntil;

				CStr Name;
				if (pSensorInfo)
					Name = pSensorInfo->m_Name;
				else
					Name = "Unknown";

				fp64 OutdatedSeconds = fp64::fs_Inf();
				auto OutdatedStatus = CDistributedAppSensorReporter::EStatusSeverity_Ok;
				if (pSensorInfo)
				{
					OutdatedStatus = Reading.m_Reading.f_OutdatedStatus(*pSensorInfo, Now, OutdatedSeconds);
					if (OutdatedStatus >= CDistributedAppSensorReporter::EStatusSeverity_Warning)
						bHasOutdated = true;
				}

				{
					auto CombinedStatus = Reading.m_Reading.f_GetCombinedStatus(pSensorInfo, Now);

					if (CombinedStatus >= CDistributedAppSensorReporter::EStatusSeverity_Error)
						Return = fg_Max(Return, 2u);
					else if (CombinedStatus >= CDistributedAppSensorReporter::EStatusSeverity_Warning)
						Return = fg_Max(Return, 1u);
				}

				if (_Flags & ESensorOutputFlag_Json)
				{
					auto Value = Reading.m_Reading.m_Data.f_VisitRet<CEJSONSorted>(gc_fSensorDataToJson);
					JsonOutputArray.f_Insert
						(
							CEJSONSorted
							{
								"HostID"_= Reading.m_SensorInfoKey.m_HostID
								, "HostName"_= HostName
								, "Application"_= Application
								, "Identifier"_= Reading.m_SensorInfoKey.m_Identifier
								, "IdentifierScope"_= Reading.m_SensorInfoKey.m_IdentifierScope
								, "Name"_= Name
								, "Timestamp"_= Reading.m_Reading.m_Timestamp
								, "UniqueSequence"_= Reading.m_Reading.m_UniqueSequence
								, "Value"_= fg_Move(Value)
								, "SnoozeUntil"_= SnoozeUntil
								, "OutdatedStatus"_= CDistributedAppSensorReporter::fs_StatusSeverityToString(OutdatedStatus)
								, "OutdatedSeconds"_= OutdatedSeconds
							}
						)
					;
				}
				else
				{
					auto DataStatus = Reading.m_Reading.f_GetDataStatus(pSensorInfo);
					auto Value = Reading.m_Reading.f_GetFormattedData
						(
							bIsSnoozed && DataStatus >= CDistributedAppSensorReporter::EStatusSeverity_Warning ? AnsiEncodingNoColor : AnsiEncoding
							, pSensorInfo
						)
					;

					CStr OutdatedString;
					if (_Flags & ESensorOutputFlag_Status)
					{
						OutdatedString = Reading.m_Reading.f_GetFormattedOutdatedStatus
							(
								bIsSnoozed && OutdatedStatus >= CDistributedAppSensorReporter::EStatusSeverity_Warning ? AnsiEncodingNoColor : AnsiEncoding
								, pSensorInfo
								, Now
								, CDistributedAppSensorReporter::EOutdatedStatusOutputFlag_IncludeStatus
							)
						;
					}
					else if (pSensorInfo && pSensorInfo->f_HasExpectedReportInterval())
						OutdatedString = "{fe0} s"_f << OutdatedSeconds;

					CStr SnoozeUntilStr;
					if (bIsSnoozed)
						SnoozeUntilStr = CStr("{}{tc6}{}"_f << AnsiEncoding.f_StatusNormal() << SnoozeUntil.f_ToLocal() << AnsiEncoding.f_Default());
					else if (SnoozeUntil.f_IsValid())
						SnoozeUntilStr = CStr("{tc6}"_f << SnoozeUntil);

					TableRenderer.f_AddRow
						(
							Reading.m_SensorInfoKey.m_HostID
							, CHostInfo::fs_FormatFriendlyNameForTable(HostName, AnsiEncoding)
							, Application
							, Reading.m_SensorInfoKey.m_Identifier
							, Reading.m_SensorInfoKey.m_IdentifierScope
							, Name
							, CStr((_Flags & ESensorOutputFlag_Verbose) ? "{tf}"_f << Reading.m_Reading.m_Timestamp.f_ToLocal() : "{}"_f << Reading.m_Reading.m_Timestamp.f_ToLocal())
							, Reading.m_Reading.m_UniqueSequence
							, Value
							, SnoozeUntilStr
							, OutdatedString
						)
					;
				}

				++nEntries;

				fOutputStatus();

				if (_MaxEntries > 0u && nEntries >= _MaxEntries)
					break;
			}
			if (_MaxEntries > 0u && nEntries >= _MaxEntries)
				break;
		}

		co_await _pCommandLine->f_StdErr("");

		if (_Flags & ESensorOutputFlag_Json)
		{
			if (_Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst)
				JsonOutputArray = JsonOutputArray.f_Reverse();

			*_pCommandLine += JsonOutput.f_ToString("\t", EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat);
		}
		else
		{
			if (!bHasHostID)
				Columns.f_SetVerbose("Host ID");
			if (!bHasHostName)
				Columns.f_SetVerbose("Host Description");
			if (!bHasApplication)
				Columns.f_SetVerbose("Application");
			if (!bHasOutdated)
				Columns.f_SetVerbose("Outdated");
			if (!bHasSnoozeUntil)
				Columns.f_SetVerbose("Snoozed Until");

			if (_Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst)
				TableRenderer.f_ReverseRows();

			TableRenderer.f_Output(_TableType);
		}

		if (_Flags & ESensorOutputFlag_Status)
			co_return Return;
		else
			co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorStatus
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppSensorReader_SensorStatusFilter const &_Filter
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenSensorStoreLocal();
		auto ReadingsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensorStatus, CDistributedAppSensorReader::CGetSensorStatus{.m_Filters = {_Filter}});
		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, CDistributedAppSensorReader::CGetSensors{.m_Filters = {_Filter.m_SensorFilter}});

		CDistributedAppSensorReader_SensorReadingFilter Filter;
		Filter.m_Flags = CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_None;

		co_return co_await self
			(
				&CDistributedAppActor::f_CommandLine_SensorReadingsOutput
				, _pCommandLine
				, fg_Move(ReadingsGenerator)
				, fg_Move(SensorsGenerator)
				, 0
				, _Flags | ESensorOutputFlag_Status
				, _TableType
				, Filter
			)
		;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorReadingsList
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppSensorReader_SensorReadingFilter const &_Filter
			, uint64 _MaxEntries
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenSensorStoreLocal();
		auto ReadingsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensorReadings, CDistributedAppSensorReader::CGetSensorReadings{.m_Filters = {_Filter}});
		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, CDistributedAppSensorReader::CGetSensors{.m_Filters = {_Filter.m_SensorFilter}});

		co_await self
			(
				&CDistributedAppActor::f_CommandLine_SensorReadingsOutput
				, _pCommandLine
				, fg_Move(ReadingsGenerator)
				, fg_Move(SensorsGenerator)
				, _MaxEntries
				, _Flags
				, _TableType
				, _Filter
			)
		;

		co_return 0;
	}
}
