// Copyright © 2022 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/Concurrency/DistributedAppLogStoreLocal>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStorage;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCommandLine;
	using namespace NEncoding;
	using namespace NTime;

	void CDistributedAppActor::fp_BuildDefaultCommandLine_DistributedLog(CDistributedAppCommandLineSpecification &o_CommandLine)
	{
		fp_BuildDefaultCommandLine_DistributedLog_Customizable
			(
				o_CommandLine.f_AddSection("Distributed Log", "Use these commands to manage the local store of distributed logs.")
				, ""
				, g_ActorFunctor / [this]
				(
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppLogReader_LogFilter const &_Filter
					, ELogOutputFlag _Flags
					, uint32 _Verbosity
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_LogList, _pCommandLine, _Filter, _Flags, _Verbosity, _TableType);
				}
				, g_ActorFunctor / [this]
				(
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppLogReader_LogEntryFilter const &_Filter
					, uint64 _MaxEntries
					, ELogOutputFlag _Flags
					, uint32 _Verbosity
					, CStr const &_TableType
				)
				-> TCFuture<uint32>
				{
					co_return co_await self(&CDistributedAppActor::f_CommandLine_LogEntriesList, _pCommandLine, _Filter, _MaxEntries, _Flags, _Verbosity, _TableType);
				}
				, EDistributedAppCommandFlag_None
			)
		;
	}

	void CDistributedAppActor::fp_BuildDefaultCommandLine_DistributedLog_Customizable
		(
			CDistributedAppCommandLineSpecification::CSection _Section
			, CStr const &_Prefix
			, TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppLogReader_LogFilter const &_Filter
					, ELogOutputFlag _Flags
					, uint32 _Verbosity
					, CStr const &_TableType
				)
			> &&_fLogList
			,
			TCActorFunctor
			<
				TCFuture<uint32>
				(
					CEJSON const &_Params
					, TCSharedPointer<CCommandLineControl> const &_pCommandLine
					, CDistributedAppLogReader_LogEntryFilter const &_Filter
					, uint64 _MaxEntries
					, ELogOutputFlag _Flags
					, uint32 _Verbosity
					, CStr const &_TableType
				)
			> && _fLogEntriesList
			, EDistributedAppCommandFlag _CommandFlags
		)
	{
		auto Option_LogHostID = "HostID?"_=
			{
				"Names"_= {"--host-id"}
				, "Type"_= ""
				, "Description"_= "Limit output to only logs from specified host id."
			}
		;

		auto Option_LogApplication = "Application?"_=
			{
				"Names"_= {"--application"}
				, "Type"_= ""
				, "Description"_= "Limit output to only logs that come from the specified application.\n"
				"Set to an empty string to only include logs that are in host scope"
			}
		;

		auto Option_LogIdentifier = "Identifier?"_=
			{
				"Names"_= {"--identifier"}
				, "Type"_= ""
				, "Description"_= "Limit output to only logs that match the identifier."
			}
		;

		auto Option_LogIdentifierScope = "IdentifierScope?"_=
			{
				"Names"_= {"--identifier-scope"}
				, "Type"_= ""
				, "Description"_= "Limit output to only logs that match the idendifier scope."
			}
		;

		auto Option_Verbose = "Verbosity?"_=
			{
				"Names"_= {"--verbosity", "-v"}
				, "Default"_= 0
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

		auto fParseLogFilter = [](CEJSON const &_Params) -> CDistributedAppLogReader_LogFilter
			{
				CDistributedAppLogReader_LogFilter Filter;

				if (auto pValue = _Params.f_GetMember("HostID"))
					Filter.m_HostID = pValue->f_String();

				if (auto pValue = _Params.f_GetMember("Application"))
				{
					auto Application = pValue->f_String();
					if (Application.f_IsEmpty())
						Filter.m_Scope = CDistributedAppLogReporter::CLogScope_Host{};
					else
						Filter.m_Scope = CDistributedAppLogReporter::CLogScope_Application{pValue->f_String()};
				}

				if (auto pValue = _Params.f_GetMember("Identifier"))
					Filter.m_Identifier = pValue->f_String();

				return Filter;
			}
		;

		auto fParseFlags = [](CEJSON const &_Params, bool _bRaw)
			{
				ELogOutputFlag Flags = ELogOutputFlag_None;
				if (_bRaw && _Params["Raw"].f_Boolean())
					Flags |= ELogOutputFlag_Raw;
				if (_Params["Json"].f_Boolean())
					Flags |= ELogOutputFlag_Json;

				return Flags;
			}
		;

		_Section.f_RegisterCommand
			(
				{
					"Names"_= {"--{}log-list"_f << _Prefix}
					, "Description"_= "List the logs that are in the local store.\n"
					, "Output"_= "A table of logs."
					, "Options"_=
					{
						Option_LogHostID
						, Option_LogApplication
						, Option_LogIdentifier
						, Option_Verbose
						, Option_Json
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fLogList = fg_Move(_fLogList), fParseLogFilter, fParseFlags](CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					co_return co_await fLogList
						(
							_Params
							, _pCommandLine
							, fParseLogFilter(_Params)
							, fParseFlags(_Params, false)
							, _Params["Verbosity"].f_Integer()
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
					"Names"_= {"--{}log-entries-list"_f << _Prefix}
					, "Description"_= "List historic values for logs that are in the local store.\n"
					, "Output"_= "A table of logs entries."
					, "Options"_=
					{
						Option_LogHostID
						, Option_LogApplication
						, Option_LogIdentifier
						, Option_Verbose
						, Option_Json
						, "Raw?"_=
						{
							"Names"_= {"--raw", "-r"}
							, "Default"_= false
							, "Description"_= "Output log entries raw."
						}
						, "FilterMessage?"_=
						{
							"Names"_= {"--filter-message"}
							, "Type"_= {""}
							, "Description"_= "Limit output to only logs entries that match any of the messages. Supports wildcards."
						}
						, "FilterSeverity?"_=
						{
							"Names"_= {"--filter-severity"}
							, "Type"_= {COneOf{"Critical", "Error", "Warning", "Info", "Debug", "DebugVerbose1", "DebugVerbose2"}}
							, "Description"_= "Limit output to only logs entries that match any of the severities."
						}
						, "FilterFlags?"_=
						{
							"Names"_= {"--filter-flags"}
							, "Type"_= {COneOf{"Audit", "Performance"}}
							, "Description"_= "Limit output to only logs entries that match any of the severities."
						}
						, "FilterCategories?"_=
						{
							"Names"_= {"--filter-categories"}
							, "Type"_= {""}
							, "Description"_= "Limit output to only logs entries that match any of the categories. Supports wildcards."
						}
						, "FilterOperations?"_=
						{
							"Names"_= {"--filter-operations"}
							, "Type"_= {""}
							, "Description"_= "Limit output to only logs entries that match any of the operations. Supports wildcards."
						}
						, "FilterSourceLocation?"_=
						{
							"Names"_= {"--filter-source-location"}
							, "Type"_= {""}
							, "Description"_= "Limit output to only logs entries that match any of the source location files. Supports wildcards."
						}
						, "MinTimestamp?"_=
						{
							"Names"_= {"--min-timestamp"}
							, "Type"_= CTime()
							, "Description"_= "Limit output to entries that are newer than specified."
						}
						, "MinSequence?"_=
						{
							"Names"_= {"--min-sequence"}
							, "Type"_= 0
							, "Description"_= "Limit output to sequences that are greater than specified for the timesstamp."
						}
						, "MaxTimestamp?"_=
						{
							"Names"_= {"--max-timestamp"}
							, "Type"_= CTime()
							, "Description"_= "Limit output to entries that are older than specified."
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
				, [fLogEntriesList = fg_Move(_fLogEntriesList), fParseLogFilter, fParseFlags]
				(CEJSON const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
				{
					CDistributedAppLogReader_LogEntryFilter Filter;
					Filter.m_LogFilter = fParseLogFilter(_Params);

					if (auto pValue = _Params.f_GetMember("MinSequence"))
						Filter.m_MinSequence = pValue->f_Integer();

					if (auto pValue = _Params.f_GetMember("MaxSequence"))
						Filter.m_MaxSequence = pValue->f_Integer();

					if (auto pValue = _Params.f_GetMember("MinTimestamp"))
						Filter.m_MinTimestamp = pValue->f_Date().f_ToUTC();

					if (auto pValue = _Params.f_GetMember("MaxTimestamp"))
						Filter.m_MaxTimestamp = pValue->f_Date().f_ToUTC();

					Filter.m_Flags = CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_None;
					if (_Params["ReportNewestFirst"].f_Boolean())
						Filter.m_Flags |= CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_ReportNewestFirst;

					auto &DataFilter = Filter.m_LogDataFilter;

					if (auto pValue = _Params.f_GetMember("FilterMessage"))
						DataFilter.m_MessageWildcards = NContainer::TCSet<NStr::CStr>().f_AddContainer(pValue->f_StringArray());

					if (auto pValue = _Params.f_GetMember("FilterCategories"))
						DataFilter.m_CategoriesWildcards = NContainer::TCSet<NStr::CStr>().f_AddContainer(pValue->f_StringArray());

					if (auto pValue = _Params.f_GetMember("FilterOperations"))
						DataFilter.m_OperationsWildcards = NContainer::TCSet<NStr::CStr>().f_AddContainer(pValue->f_StringArray());

					if (auto pValue = _Params.f_GetMember("FilterSourceLocation"))
						DataFilter.m_SourceLocationFileWildcards = NContainer::TCSet<NStr::CStr>().f_AddContainer(pValue->f_StringArray());

					if (auto pValue = _Params.f_GetMember("FilterSeverity"))
					{
						NContainer::TCSet<CDistributedAppLogReporter::ELogSeverity> Severities;
						for (auto &Severity : pValue->f_StringArray())
							Severities[CDistributedAppLogReporter::fs_LogSeverityFromStr(Severity)];

						DataFilter.m_Severities = fg_Move(Severities);
					}

					if (auto pValue = _Params.f_GetMember("FilterFlags"))
					{
						CDistributedAppLogReporter::ELogEntryFlag Flags = CDistributedAppLogReporter::ELogEntryFlag_None;
						for (auto &Flag : pValue->f_StringArray())
						{
							if (Flag == "Performance")
								Flags |= CDistributedAppLogReporter::ELogEntryFlag_Performance;
							else if (Flag == "Audit")
								Flags |= CDistributedAppLogReporter::ELogEntryFlag_Audit;
						}

						DataFilter.m_Flags = Flags;
					}

					co_return co_await fLogEntriesList
						(
							_Params
							, _pCommandLine
							, Filter
							, _Params["MaxEntries"].f_Integer()
							, fParseFlags(_Params, true)
							, _Params["Verbosity"].f_Integer()
							, _Params["TableType"].f_String()
						)
					;
				}
				, _CommandFlags
			)
		;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_LogListOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
			, ELogOutputFlag _Flags
			, uint32 _Verbosity
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

		TableRenderer.f_AddHeadingsVector(Headings);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;

		CEJSON JsonOutput;
		auto &JsonOutputArray = JsonOutput.f_Array();

		for (auto iLogs = co_await fg_Move(_Logs).f_GetIterator(); iLogs; co_await ++iLogs)
		{
			for (auto &LogInfo : *iLogs)
			{
				CStr Application;
				if (LogInfo.m_Scope.f_IsOfType<CDistributedAppLogReporter::CLogScope_Application>())
					Application = LogInfo.m_Scope.f_GetAsType<CDistributedAppLogReporter::CLogScope_Application>().m_ApplicationName;

				if (_Flags & ELogOutputFlag_Json)
				{
					JsonOutputArray.f_Insert
						(
							CEJSON
							{
								"HostID"_= LogInfo.m_HostID
								, "HostName"_= LogInfo.m_HostName
								, "Application"_= Application
								, "Identifier"_= LogInfo.m_Identifier
								, "IdentifierScope"_= LogInfo.m_IdentifierScope
								, "Name"_= LogInfo.m_Name
							}
						)
					;
				}
				else
				{
					bHasHostID = bHasHostID || LogInfo.m_HostID;
					bHasHostName = bHasHostName || LogInfo.m_HostName;
					bHasApplication = bHasApplication || Application;

					TableRenderer.f_AddRow
						(
							LogInfo.m_HostID
							, LogInfo.m_HostName
							, Application
							, LogInfo.m_Identifier
							, LogInfo.m_IdentifierScope
							, LogInfo.m_Name
						)
					;
				}
			}
		}

		if (_Flags & ELogOutputFlag_Json)
			*_pCommandLine += JsonOutput.f_ToString();
		else
		{
			if (!_Verbosity)
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

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_LogList
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppLogReader_LogFilter const &_Filter
			, ELogOutputFlag _Flags
			, uint32 _Verbosity
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenLogStoreLocal();

		auto LogsGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogs, fg_TempCopy(_Filter), 1024);

		co_return co_await self
			(
				&CDistributedAppActor::f_CommandLine_LogListOutput
				, _pCommandLine
				, fg_Move(LogsGenerator)
				, _Flags
				, _Verbosity
				, _TableType
			)
		;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_LogEntriesOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> &&_LogEntries
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
			, uint64 _MaxEntries
			, ELogOutputFlag _Flags
			, uint32 _Verbosity
			, CStr const &_TableType
			, CDistributedAppLogReader_LogEntryFilter const &_Filter
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
					&CDistributedAppActor::fp_CommandLine_LogEntriesOutput
					, _pCommandLine
					, fg_Move(_LogEntries)
					, fg_Move(_Logs)
					, _MaxEntries
					, _Flags
					, _Verbosity
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

	TCFuture<uint32> CDistributedAppActor::fp_CommandLine_LogEntriesOutput
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReader_LogKeyAndEntry>> &&_LogEntries
			, TCAsyncGenerator<NContainer::TCVector<CDistributedAppLogReporter::CLogInfo>> &&_Logs
			, uint64 _MaxEntries
			, ELogOutputFlag _Flags
			, uint32 _Verbosity
			, CStr const &_TableType
			, CDistributedAppLogReader_LogEntryFilter const &_Filter
		)
	{
		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		TCMap<CDistributedAppLogReporter::CLogInfoKey, CDistributedAppLogReporter::CLogInfo> LogInfos;
		{
			for (auto iLogs = co_await fg_Move(_Logs).f_GetIterator(); iLogs; co_await ++iLogs)
			{
				for (auto &LogInfo : *iLogs)
					LogInfos[LogInfo.f_Key()] = LogInfo;
			}
		}

		TCVector<CStr> Headings;
		TCMap<mint, uint32> VerboseHeadings;
		TCMap<CStr, mint> HeadingIndices;

		auto fAddHeading = [&](CStr const &_Name, uint32 _Verbosity)
			{
				auto HeadingIndex = Headings.f_GetLen();
				HeadingIndices[_Name] = HeadingIndex;
				if (_Verbosity)
					VerboseHeadings[HeadingIndex] = _Verbosity;

				Headings.f_Insert(_Name);
			}
		;
		auto fSetVerbose = [&](CStr const &_Heading, uint32 _Verbosity)
			{
				DMibRequire(HeadingIndices.f_FindEqual(_Heading));
				VerboseHeadings[HeadingIndices[_Heading]] = _Verbosity;
			}
		;

		fAddHeading("Host ID", 5);
		fAddHeading("Host Name", 0);
		fAddHeading("Application", 0);
		fAddHeading("Identifier", 4);
		fAddHeading("Identifier Scope", 5);
		fAddHeading("Name", 0);
		fAddHeading("Timestamp", 0);
		fAddHeading("Sequence", 4);
		fAddHeading("Source Location", 6);
		fAddHeading("Categories", 0);
		fAddHeading("Operations", 3);
		fAddHeading("Severity", 0);
		fAddHeading("Flags", 1);
		fAddHeading("Message", 0);
		fAddHeading("Metadata", 2);

		TableRenderer.f_AddHeadingsVector(Headings);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;
		bool bHasName = false;
		uint64 nEntries = 0;

		CStr TimeColor = AnsiEncoding.f_ForegroundRGB(160, 160, 160);
		CStr DebugColor = AnsiEncoding.f_ForegroundRGB(100, 100, 100);
		CStr CategoryColor = AnsiEncoding.f_ForegroundRGB(51, 182, 255);

		CEJSON JsonOutput;
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

		for (auto iEntries = co_await fg_Move(_LogEntries).f_GetIterator(); iEntries; co_await ++iEntries)
		{
			for (auto &Entry : *iEntries)
			{
				CStr Application;
				if (Entry.m_LogInfoKey.m_Scope.f_IsOfType<CDistributedAppLogReporter::CLogScope_Application>())
					Application = Entry.m_LogInfoKey.m_Scope.f_GetAsType<CDistributedAppLogReporter::CLogScope_Application>().m_ApplicationName;

				CStr HostName;
				auto *pLogInfo = LogInfos.f_FindEqual(Entry.m_LogInfoKey);
				if (pLogInfo)
					HostName = pLogInfo->m_HostName;

				CStr Name;
				if (pLogInfo)
					Name = pLogInfo->m_Name;

				bHasHostID = bHasHostID || Entry.m_LogInfoKey.m_HostID;
				bHasHostName = bHasHostName || HostName;
				bHasApplication = bHasApplication || Application;
				bHasName = bHasName || (Name && Name != "Malterlib Distributed App");

				auto &Data = Entry.m_Entry.m_Data;

				if (_Flags & ELogOutputFlag_Raw)
					*_pCommandLine += CStrSecure::CFormat("{}\n") << Data.m_Message;
				else if (_Flags & ELogOutputFlag_Json)
				{
					JsonOutputArray.f_Insert
						(
							CEJSON
							{
								"HostID"_= Entry.m_LogInfoKey.m_HostID
								, "HostName"_= HostName
								, "Application"_= Application
								, "Identifier"_= Entry.m_LogInfoKey.m_Identifier
								, "IdentifierScope"_= Entry.m_LogInfoKey.m_IdentifierScope
								, "Name"_= Name
								, "Timestamp"_= Entry.m_Entry.m_Timestamp
								, "UniqueSequence"_= Entry.m_Entry.m_UniqueSequence
								, "Data"_= Data.f_ToJSON()
							}
						)
					;
				}
				else
				{
					CStr SeverityColor;
					if (pLogInfo)
					{
						switch (Data.m_Severity)
						{
						case CDistributedAppLogReporter::ELogSeverity_Critical:
						case CDistributedAppLogReporter::ELogSeverity_Error:
							SeverityColor = AnsiEncoding.f_StatusError();
							break;
						case CDistributedAppLogReporter::ELogSeverity_Warning:
							SeverityColor = AnsiEncoding.f_StatusWarning();
							break;
						case CDistributedAppLogReporter::ELogSeverity_Info:
							SeverityColor = AnsiEncoding.f_StatusNormal();
							break;
						case CDistributedAppLogReporter::ELogSeverity_Unsupported:
						case CDistributedAppLogReporter::ELogSeverity_Debug:
						case CDistributedAppLogReporter::ELogSeverity_DebugVerbose1:
						case CDistributedAppLogReporter::ELogSeverity_DebugVerbose2:
						case CDistributedAppLogReporter::ELogSeverity_DebugVerbose3:
							SeverityColor = DebugColor;
							break;
						}
					}

					CStr Severity = CDistributedAppLogReporter::fs_LogSeverityToStr(Data.m_Severity);

					if (SeverityColor)
						Severity = ("{}{}{}"_f << SeverityColor << Severity << AnsiEncoding.f_Default()).f_GetStr();

					CStr TimeStamp;
					if (_Verbosity >= 4)
						TimeStamp = "{tf}"_f << Entry.m_Entry.m_Timestamp.f_ToLocal();
					else
						TimeStamp = "{}"_f << Entry.m_Entry.m_Timestamp.f_ToLocal();

					if (TimeColor)
						TimeStamp = ("{}{}{}"_f << TimeColor << TimeStamp << AnsiEncoding.f_Default()).f_GetStr();

					CStr Categories = CStr::fs_Join(Data.m_Categories, "\n");
					CStr Operations = CStr::fs_Join(Data.m_Operations, "\n");

					if (CategoryColor)
						Categories = ("{}{}{}"_f << CategoryColor << Categories << AnsiEncoding.f_Default()).f_GetStr();

					CStr SourceLocation;
					if (Data.m_SourceLocation)
					{
						auto &Location = *Data.m_SourceLocation;
						SourceLocation = CStr::CFormat(DMibPFileLineFormat) << Location.m_File << Location.m_Line;
					}

					CStr Flags;
					if (Data.m_Flags & CDistributedAppLogReporter::ELogEntryFlag_Performance)
						fg_AddStrSep(Flags, "Performance", "\n");
					if (Data.m_Flags & CDistributedAppLogReporter::ELogEntryFlag_Audit)
						fg_AddStrSep(Flags, "Audit", "\n");

					CStr MetaData;
					if (Data.m_MetaData.f_IsValid())
						MetaData = Data.m_MetaData.f_ToString();

					TableRenderer.f_AddRow
						(
							Entry.m_LogInfoKey.m_HostID
							, HostName
							, Application
							, Entry.m_LogInfoKey.m_Identifier
							, Entry.m_LogInfoKey.m_IdentifierScope
							, Name
							, TimeStamp
							, Entry.m_Entry.m_UniqueSequence
							, SourceLocation
							, Categories
							, Operations
							, Severity
							, Flags
							, Entry.m_Entry.m_Data.m_Message
							, MetaData
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

		if (_Flags & ELogOutputFlag_Raw)
			;
		else if (_Flags & ELogOutputFlag_Json)
		{
			if (_Filter.m_Flags & CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_ReportNewestFirst)
				JsonOutputArray = JsonOutputArray.f_Reverse();

			*_pCommandLine += JsonOutput.f_ToString();
		}
		else
		{
			if (!bHasName)
				fSetVerbose("Name", 7);
			if (!bHasHostID)
				fSetVerbose("Host ID", 7);
			if (!bHasHostName)
				fSetVerbose("Host Name", 7);
			if (!bHasApplication)
				fSetVerbose("Application", 7);

			while (auto pVerbosityLevel = VerboseHeadings.f_FindLargest())
			{
				if (*pVerbosityLevel > _Verbosity)
					TableRenderer.f_RemoveColumn(VerboseHeadings.fs_GetKey(*pVerbosityLevel));
				VerboseHeadings.f_Remove(pVerbosityLevel);
			}

			if (_Filter.m_Flags & CDistributedAppLogReader_LogEntryFilter::ELogEntriesFlag_ReportNewestFirst)
				TableRenderer.f_ReverseRows();

			TableRenderer.f_Output(_TableType);
		}

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_LogEntriesList
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CDistributedAppLogReader_LogEntryFilter const &_Filter
			, uint64 _MaxEntries
			, ELogOutputFlag _Flags
			, uint32 _Verbosity
			, CStr const &_TableType
		)
	{
		auto LocalStore = co_await fp_OpenLogStoreLocal();
		auto EntriesGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogEntries, fg_TempCopy(_Filter), 1024);
		auto LogsGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogs, fg_TempCopy(_Filter.m_LogFilter), 1024);

		co_await self
			(
				&CDistributedAppActor::f_CommandLine_LogEntriesOutput
				, _pCommandLine
				, fg_Move(EntriesGenerator)
				, fg_Move(LogsGenerator)
				, _MaxEntries
				, _Flags
				, _Verbosity
				, _TableType
				, _Filter
			)
		;

		co_return 0;
	}
}

