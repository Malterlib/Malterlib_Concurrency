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
					CEJSON const &_Params
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
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_SensorStatus, _pCommandLine, _Filter, _Flags, _TableType);
				}
				, g_ActorFunctor / [this]
				(
					CEJSON const &_Params
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
					CEJSON const &_Params
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
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppSensorReader_SensorFilter const &_Filter
					, ESensorOutputFlag _Flags
					, CStr const &_TableType
				)
			> &&_fSensorStatus
			,
			TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSON const &_Params
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
		auto Option_SensorHostID = "HostID?"_=
			{
				"Names"_= {"--host-id"}
				, "Type"_= ""
				, "Description"_= "Limit output to only sensors from specified host id."
			}
		;

		auto Option_SensorApplication = "Application?"_=
			{
				"Names"_= {"--application"}
				, "Type"_= ""
				, "Description"_= "Limit output to only sensors that come from the specified application.\n"
				"Set to an empty string to only include sensors that are in host scope"
			}
		;

		auto Option_SensorIdentifier = "Identifier?"_=
			{
				"Names"_= {"--identifier"}
				, "Type"_= ""
				, "Description"_= "Limit output to only sensors that match the identifier."
			}
		;

		auto Option_SensorIdentifierScope = "IdentifierScope?"_=
			{
				"Names"_= {"--identifier-scope"}
				, "Type"_= ""
				, "Description"_= "Limit output to only sensors that match the idendifier scope."
			}
		;

		auto Option_Verbose = "Verbose?"_=
			{
				"Names"_= {"--verbose", "-v"}
				, "Default"_= false
				, "Description"_= "Output all properties."
			}
		;

		auto Option_Json = "Json?"_=
			{
				"Names"_= {"--json", "-j"}
				, "Default"_= false
				, "Description"_= "Output in json format."
			}
		;

		auto fParseSensorFilter = [](CEJSON const &_Params) -> CDistributedAppSensorReader_SensorFilter
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

				return Filter;
			}
		;

		auto fParseFlags = [](CEJSON const &_Params)
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
					"Names"_= {"--{}sensor-list"_f << _Prefix}
					, "Description"_= "List the sensors that are in the local store.\n"
					, "Output"_= "A table of sensors."
					, "Options"_=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_Verbose
						, Option_Json
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorList = fg_Move(_fSensorList), fParseSensorFilter, fParseFlags](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
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
					"Names"_= {"--{}sensor-status"_f << _Prefix}
					, "Description"_= "List the current value for sensors that are in the local store.\n"
					, "Output"_= "A table of sensors readings."
					, "Options"_=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_Verbose
						, Option_Json
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorStatus = fg_Move(_fSensorStatus), fParseSensorFilter, fParseFlags]
				(CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					co_return co_await fSensorStatus
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
					"Names"_= {"--{}sensor-readings-list"_f << _Prefix}
					, "Description"_= "List historic values for sensors that are in the local store.\n"
					, "Output"_= "A table of sensors readings."
					, "Options"_=
					{
						Option_SensorHostID
						, Option_SensorApplication
						, Option_SensorIdentifier
						, Option_Verbose
						, Option_Json
						, "MinTimestamp?"_=
						{
							"Names"_= {"--min-timestamp"}
							, "Type"_= CTime()
							, "Description"_= "Limit output to readings that are newer than specified."
						}
						, "MinSequence?"_=
						{
							"Names"_= {"--min-sequence"}
							, "Type"_= 0
							, "Description"_= "Limit output to sequences that are greater than specified for the timestamp."
						}
						, "MaxTimestamp?"_=
						{
							"Names"_= {"--max-timestamp"}
							, "Type"_= CTime()
							, "Description"_= "Limit output to readings that are older than specified."
						}
						, "MaxSequence?"_=
						{
							"Names"_= {"--max-sequence"}
							, "Type"_= 0
							, "Description"_= "Limit output to sequences that are less than specified for the timestamp."
						}
						, "ReportNewestFirst?"_=
						{
							"Names"_= {"--newest"}
							, "Default"_= true
							, "Description"_= "When applying entry limit, show newest entries first."
						}
						, "MaxEntries?"_=
						{
							"Names"_= {"--max-entries"}
							, "Default"_= 100
							, "Description"_= "Limit the number of output lines.\n"
							"Set to 0 for unlimited entries"
						}
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fSensorReadingsList = fg_Move(_fSensorReadingsList), fParseSensorFilter, fParseFlags]
				(CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
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
		auto constexpr gc_fSensorDataToJSON = [](auto const &_Value) -> CEJSON
			{
				using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
				if constexpr (NTraits::TCIsSame<CValueType, CVoidTag>::mc_Value)
					return nullptr;
				else if constexpr (NTraits::TCIsSame<CValueType, CDistributedAppSensorReporter::CStatus>::mc_Value)
				{
					return CEJSONUserType
						(
							"Status"
							,
							{
								"Severity"__= _Value.m_Severity
								, "Description"__= _Value.m_Description
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

		TCVector<CStr> Headings;
		TCSet<mint> VerboseHeadings;
		TCMap<CStr, mint> HeadingIndices;

		auto fAddHeading = [&](CStr const &_Name, bool _bVerbose = true)
			{
				auto HeadingIndex = Headings.f_GetLen();
				HeadingIndices[_Name] = HeadingIndex;
				if (_bVerbose)
					VerboseHeadings[HeadingIndex];

				Headings.f_Insert(_Name);
			}
		;
		auto fSetVerbose = [&](CStr const &_Heading)
			{
				DMibRequire(HeadingIndices.f_FindEqual(_Heading));
				VerboseHeadings[HeadingIndices[_Heading]];
			}
		;

		fAddHeading("Host ID", false);
		fAddHeading("Host Name", false);
		fAddHeading("Application", false);
		fAddHeading("Identifier", false);
		fAddHeading("Identifier Scope", false);
		fAddHeading("Name", false);
		fAddHeading("Type");
		fAddHeading("Expected Report Interval");
		fAddHeading("Unit Divisors");
		fAddHeading("Warn Value");
		fAddHeading("Critical Value");

		TableRenderer.f_AddHeadingsVector(Headings);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;

		CEJSON JsonOutput;
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
					CEJSON UnitDivisors;
					auto &UnitDivisorsArray = UnitDivisors.f_Array();
					for (auto &Divisor : SensorInfo.m_UnitDivisors)
					{
						UnitDivisorsArray.f_Insert
							(
	 							CEJSON
								{
									"Divisor"_= SensorInfo.m_UnitDivisors.fs_GetKey(Divisor).f_VisitRet<CEJSON>(gc_fSensorDataToJSON)
									, "nDecimals"_= Divisor.m_nDecimals
									, "UnitFormatter"_= Divisor.m_UnitFormatter
								}
							)
						;
					}

					auto fComparisonValue = [](NStorage::TCOptional<CDistributedAppSensorReporter::CValueComparison> const &_Value) -> CEJSON
						{
							if (!_Value)
								return nullptr;

							auto &Value = *_Value;

							return
								{
									"CompareToValue"_= Value.m_CompareToValue.f_VisitRet<CEJSON>(gc_fSensorDataToJSON)
									, "Operator"_= Value.m_Operator
								}
							;
						}
					;

					JsonOutputArray.f_Insert
						(
							CEJSON
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
							, SensorInfo.m_HostName
							, Application
							, SensorInfo.m_Identifier
							, SensorInfo.m_IdentifierScope
							, SensorInfo.m_Name
							, CDistributedAppSensorReporter::fs_SensorDataTypeToString(SensorInfo.m_Type)
							, SensorInfo.m_ExpectedReportInterval
							, CDistributedAppSensorReporter::fs_FormatSensorDivisors(SensorInfo.m_UnitDivisors, "\n")
							, SensorInfo.m_WarnValue
							, SensorInfo.m_CriticalValue
						)
					;
				}
			}
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");

		if (_Flags & ESensorOutputFlag_Json)
			*_pCommandLine += JsonOutput.f_ToString();
		else
		{
			if (!(_Flags & ESensorOutputFlag_Verbose))
			{
				if (!bHasHostID)
					fSetVerbose("Host ID");
				if (!bHasHostName)
					fSetVerbose("Host Name");
				if (!bHasApplication)
					fSetVerbose("Application");

				while (auto pLargest = VerboseHeadings.f_FindLargest())
				{
					TableRenderer.f_RemoveColumn(*pLargest);
					VerboseHeadings.f_Remove(pLargest);
				}
			}

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

		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, fg_TempCopy(_Filter), 1024);

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
		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		TCMap<CDistributedAppSensorReporter::CSensorInfoKey, CDistributedAppSensorReporter::CSensorInfo> SensorInfos;
		{
			for (auto iSensors = co_await fg_Move(_Sensors).f_GetIterator(); iSensors; co_await ++iSensors)
			{
				for (auto &SensorInfo : *iSensors)
					SensorInfos[SensorInfo.f_Key()] = SensorInfo;
			}
		}

		TCVector<CStr> Headings;
		TCSet<mint> VerboseHeadings;
		TCMap<CStr, mint> HeadingIndices;

		auto fAddHeading = [&](CStr const &_Name, bool _bVerbose = true)
			{
				auto HeadingIndex = Headings.f_GetLen();
				HeadingIndices[_Name] = HeadingIndex;
				if (_bVerbose)
					VerboseHeadings[HeadingIndex];

				Headings.f_Insert(_Name);
			}
		;
		auto fSetVerbose = [&](CStr const &_Heading)
			{
				DMibRequire(HeadingIndices.f_FindEqual(_Heading));
				VerboseHeadings[HeadingIndices[_Heading]];
			}
		;

		fAddHeading("Host ID");
		fAddHeading("Host Name", false);
		fAddHeading("Application", false);
		fAddHeading("Identifier");
		fAddHeading("Identifier Scope");
		fAddHeading("Name", false);
		fAddHeading("Timestamp", false);
		fAddHeading("Sequence");
		fAddHeading("Value", false);

		TableRenderer.f_AddHeadingsVector(Headings);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;
		uint64 nEntries = 0;

		CEJSON JsonOutput;
		auto &JsonOutputArray = JsonOutput.f_Array();

		uint32 Return = 0;

		for (auto iReadings = co_await fg_Move(_SensorReadings).f_GetIterator(); iReadings; co_await ++iReadings)
		{
			for (auto &Reading : *iReadings)
			{
				CStr Application;
				if (Reading.m_SensorInfoKey.m_Scope.f_IsOfType<CDistributedAppSensorReporter::CSensorScope_Application>())
					Application = Reading.m_SensorInfoKey.m_Scope.f_GetAsType<CDistributedAppSensorReporter::CSensorScope_Application>().m_ApplicationName;

				CStr HostName;
				auto *pSensorInfo = SensorInfos.f_FindEqual(Reading.m_SensorInfoKey);
				if (pSensorInfo)
					HostName = pSensorInfo->m_HostName;

				bHasHostID = bHasHostID || Reading.m_SensorInfoKey.m_HostID;
				bHasHostName = bHasHostName || HostName;
				bHasApplication = bHasApplication || Application;

				CStr Name;
				if (pSensorInfo)
					Name = pSensorInfo->m_Name;
				else
					Name = "Unknown";

				if (_Flags & ESensorOutputFlag_Json)
				{
					auto Value = Reading.m_Reading.m_Data.f_VisitRet<CEJSON>(gc_fSensorDataToJSON);
					JsonOutputArray.f_Insert
						(
 							CEJSON
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
							}
						)
					;
				}
				else
				{
					CStr Value;

					CStr ValueColor;
					if (pSensorInfo)
					{
						if (pSensorInfo->f_IsValueCritical(Reading.m_Reading.m_Data))
						{
							ValueColor = AnsiEncoding.f_StatusError();
							Return = fg_Max(Return, 2u);
						}
						else if (pSensorInfo->f_IsValueWarning(Reading.m_Reading.m_Data))
						{
							ValueColor = AnsiEncoding.f_StatusWarning();
							Return = fg_Max(Return, 1u);
						}
						else if (pSensorInfo->f_HasValueWarnings())
							ValueColor = AnsiEncoding.f_StatusNormal();
					}

					Reading.m_Reading.m_Data.f_Visit
						(
							[&](auto const &_Value)
							{
								using CValueType = typename NTraits::TCRemoveReferenceAndQualifiers<decltype(_Value)>::CType;
								if constexpr (NTraits::TCIsSame<CValueType, CVoidTag>::mc_Value)
									Value = "Unsupported";
								else if constexpr (NTraits::TCIsSame<CValueType, bool>::mc_Value)
									Value = _Value ? "true" : "false";
								else if constexpr (NTraits::TCIsSame<CValueType, CDistributedAppSensorReporter::CStatus>::mc_Value)
								{
									Value = _Value.m_Description;

									switch (_Value.m_Severity)
									{
									case CDistributedAppSensorReporter::EStatusSeverity_Ok:
										if (!ValueColor)
											ValueColor = AnsiEncoding.f_StatusNormal();
										break;
									case CDistributedAppSensorReporter::EStatusSeverity_Info:
										break;
									case CDistributedAppSensorReporter::EStatusSeverity_Warning:
										if (!ValueColor)
											ValueColor = AnsiEncoding.f_StatusWarning();
										break;
									case CDistributedAppSensorReporter::EStatusSeverity_Error:
										if (!ValueColor)
											ValueColor = AnsiEncoding.f_StatusError();
										break;
									}
								}
								else
								{
									if (pSensorInfo)
									{
										auto *pDivisorUnit = pSensorInfo->m_UnitDivisors.f_FindLargestLessThanEqual(CDistributedAppSensorReporter::CSensorData(fg_Abs(_Value)));
										if (!pDivisorUnit)
											pDivisorUnit = pSensorInfo->m_UnitDivisors.f_FindLargestLessThanEqual(CDistributedAppSensorReporter::CSensorData(fp64(fg_Abs(_Value))));

										if (pDivisorUnit)
										{
											auto &Divisor = pSensorInfo->m_UnitDivisors.fs_GetKey(pDivisorUnit);
											if (Divisor.template f_IsOfType<CValueType>())
											{
												if (Divisor.template f_GetAsType<CValueType>())
													Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << (_Value / Divisor.template f_GetAsType<CValueType>());
												else
													Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << _Value;
												return;
											}
											else if (Divisor.template f_IsOfType<fp64>())
											{
												// Allow floating point division for integer data

												if (Divisor.template f_GetAsType<fp64>())
													Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << (fp64(_Value) / Divisor.template f_GetAsType<fp64>());
												else
													Value = CStr::CFormat(pDivisorUnit->m_UnitFormatter) << fp64(_Value);
												return;
											}
										}
									}
									Value = "{}"_f << _Value;
								}
							}
						)
					;

					if (ValueColor)
						Value = ("{}{}{}"_f << ValueColor << Value << AnsiEncoding.f_Default()).f_GetStr();

					CStr TimeStamp;
					if (_Flags & ESensorOutputFlag_Verbose)
						TimeStamp = "{tf}"_f << Reading.m_Reading.m_Timestamp.f_ToLocal();
					else
						TimeStamp = "{}"_f << Reading.m_Reading.m_Timestamp.f_ToLocal();

					TableRenderer.f_AddRow
						(
							Reading.m_SensorInfoKey.m_HostID
							, HostName
							, Application
							, Reading.m_SensorInfoKey.m_Identifier
							, Reading.m_SensorInfoKey.m_IdentifierScope
							, Name
							, TimeStamp
							, Reading.m_Reading.m_UniqueSequence
							, Value
						)
					;
				}

				++nEntries;
				if (_MaxEntries > 0u && nEntries >= _MaxEntries)
					break;
			}
			if (_MaxEntries > 0u && nEntries >= _MaxEntries)
				break;
		}

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported listen addresses to command line");

		if (_Flags & ESensorOutputFlag_Json)
		{
			if (_Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst)
				JsonOutputArray = JsonOutputArray.f_Reverse();

			*_pCommandLine += JsonOutput.f_ToString();
		}
		else
		{
			if (!(_Flags & ESensorOutputFlag_Verbose))
			{
				if (!bHasHostID)
					fSetVerbose("Host ID");
				if (!bHasHostName)
					fSetVerbose("Host Name");
				if (!bHasApplication)
					fSetVerbose("Application");

				while (auto pLargest = VerboseHeadings.f_FindLargest())
				{
					TableRenderer.f_RemoveColumn(*pLargest);
					VerboseHeadings.f_Remove(pLargest);
				}
			}

			if (_Filter.m_Flags & CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_ReportNewestFirst)
				TableRenderer.f_ReverseRows();

			TableRenderer.f_Output(_TableType);
		}

		co_return Return;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SensorStatus
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppSensorReader_SensorFilter const &_Filter
			, ESensorOutputFlag _Flags
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenSensorStoreLocal();
		auto ReadingsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensorStatus, fg_TempCopy(_Filter), 1024);
		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, fg_TempCopy(_Filter), 1024);

		CDistributedAppSensorReader_SensorReadingFilter Filter;
		Filter.m_Flags = CDistributedAppSensorReader_SensorReadingFilter::ESensorReadingsFlag_None;

		co_return co_await self
			(
				&CDistributedAppActor::f_CommandLine_SensorReadingsOutput
				, _pCommandLine
				, fg_Move(ReadingsGenerator)
				, fg_Move(SensorsGenerator)
				, 0
				, _Flags
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
		auto ReadingsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensorReadings, fg_TempCopy(_Filter), 1024);
		auto SensorsGenerator = co_await LocalStore(&CDistributedAppSensorStoreLocal::f_GetSensors, fg_TempCopy(_Filter.m_SensorFilter), 1024);

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
