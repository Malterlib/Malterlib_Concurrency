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
					CEJSONSorted const &_Params
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
					CEJSONSorted const &_Params
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
					CEJSONSorted const &_Params
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
					CEJSONSorted const &_Params
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
		auto Option_LogHostID = "HostID?"_o=
			{
				"Names"_o= {"--host-id"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only logs from specified host id."
			}
		;

		auto Option_LogApplication = "Application?"_o=
			{
				"Names"_o= {"--application"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only logs that come from the specified application. Supports wildcards.\n"
				"Set to an empty string to only include logs that are in host scope."
			}
		;

		auto Option_LogIdentifier = "Identifier?"_o=
			{
				"Names"_o= {"--identifier"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only logs that match the identifier. Supports wildcards."
			}
		;

		auto Option_LogIdentifierScope = "IdentifierScope?"_o=
			{
				"Names"_o= {"--identifier-scope"}
				, "Type"_o= ""
				, "Description"_o= "Limit output to only logs that match the idendifier scope. Supports wildcards."
			}
		;

		auto Option_Verbose = "Verbosity?"_o=
			{
				"Names"_o= {"--verbosity", "-v"}
				, "Default"_o= 0
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

		auto fOption_IgnoreRemoved = [](bool _bDefault)
			{
				return "IgnoreRemoved?"_o=
					{
						"Names"_o= {"--ignore-removed"}
						, "Default"_o= _bDefault
						, "Description"_o= "Ignore logs that were removed, or that were owned by a removed application or app manager."
					}
				;
			}
		;

		auto fParseLogFilter = [](CEJSONSorted const &_Params) -> CDistributedAppLogReader_LogFilter
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

				if (auto pValue = _Params.f_GetMember("IdentifierScope"))
					Filter.m_IdentifierScope = pValue->f_String();
					
				if (auto pValue = _Params.f_GetMember("IgnoreRemoved"); pValue->f_Boolean())
					Filter.m_Flags |= CDistributedAppLogReader_LogFilter::ELogFlag_IgnoreRemoved;

				return Filter;
			}
		;

		auto fParseFlags = [](CEJSONSorted const &_Params, bool _bRaw)
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
					"Names"_o= {"--{}log-list"_f << _Prefix}
					, "Description"_o= "List the logs that are in the local store.\n"
					, "Output"_o= "A table of logs."
					, "Options"_o=
					{
						Option_LogHostID
						, Option_LogApplication
						, Option_LogIdentifier
						, Option_LogIdentifierScope
						, fOption_IgnoreRemoved(false)
						, Option_Verbose
						, Option_Json
						, CTableRenderHelper::fs_OutputTypeOption()
					}
				}
				, [fLogList = fg_Move(_fLogList), fParseLogFilter, fParseFlags](CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
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
					"Names"_o= {"--{}log-entries-list"_f << _Prefix}
					, "Description"_o= "List historic values for logs that are in the local store.\n"
					, "Output"_o= "A table of logs entries."
					, "Options"_o=
					{
						Option_LogHostID
						, Option_LogApplication
						, Option_LogIdentifier
						, Option_LogIdentifierScope
						, fOption_IgnoreRemoved(false)
						, Option_Verbose
						, Option_Json
						, "Raw?"_o=
						{
							"Names"_o= {"--raw", "-r"}
							, "Default"_o= false
							, "Description"_o= "Output log entries raw."
						}
						, "FilterMessage?"_o=
						{
							"Names"_o= {"--filter-message"}
							, "Type"_o= {""}
							, "Description"_o= "Limit output to only logs entries that match any of the messages. Supports wildcards."
						}
						, "FilterSeverity?"_o=
						{
							"Names"_o= {"--filter-severity"}
							, "Type"_o= {COneOf{"Critical", "Error", "Warning", "Info", "Debug", "DebugVerbose1", "DebugVerbose2"}}
							, "Description"_o= "Limit output to only logs entries that match any of the severities."
						}
						, "FilterFlags?"_o=
						{
							"Names"_o= {"--filter-flags"}
							, "Type"_o= {COneOf{"Audit", "Performance"}}
							, "Description"_o= "Limit output to only logs entries that match any of the severities."
						}
						, "FilterCategories?"_o=
						{
							"Names"_o= {"--filter-categories"}
							, "Type"_o= {""}
							, "Description"_o= "Limit output to only logs entries that match any of the categories. Supports wildcards."
						}
						, "FilterOperations?"_o=
						{
							"Names"_o= {"--filter-operations"}
							, "Type"_o= {""}
							, "Description"_o= "Limit output to only logs entries that match any of the operations. Supports wildcards."
						}
						, "FilterSourceLocation?"_o=
						{
							"Names"_o= {"--filter-source-location"}
							, "Type"_o= {""}
							, "Description"_o= "Limit output to only logs entries that match any of the source location files. Supports wildcards."
						}
						, "MinTimestamp?"_o=
						{
							"Names"_o= {"--min-timestamp"}
							, "Type"_o= CTime()
							, "Description"_o= "Limit output to entries that are newer than specified."
						}
						, "MinSequence?"_o=
						{
							"Names"_o= {"--min-sequence"}
							, "Type"_o= 0
							, "Description"_o= "Limit output to sequences that are greater than specified for the timesstamp."
						}
						, "MaxTimestamp?"_o=
						{
							"Names"_o= {"--max-timestamp"}
							, "Type"_o= CTime()
							, "Description"_o= "Limit output to entries that are older than specified."
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
				, [fLogEntriesList = fg_Move(_fLogEntriesList), fParseLogFilter, fParseFlags]
				(CEJSONSorted const &_Params, TCSharedPointer<CCommandLineControl> const &_pCommandLine) -> TCFuture<uint32>
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

	static CEJSONSorted fg_MetaDataToJson(TCMap<CStr, CEJSONSorted> const &_MetaData)
	{
		CEJSONSorted Return = EJSONType_Object;

		for (auto &Entry : _MetaData.f_Entries())
			Return[Entry.f_Key()] = Entry.f_Value();

		return Return;
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
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		CTableRenderHelper::CColumnHelper Columns(_Verbosity);

		Columns.f_AddHeading("Host ID", 0);
		Columns.f_AddHeading("Host Description", 0);
		Columns.f_AddHeading("Application", 0);
		Columns.f_AddHeading("Identifier", 0);
		Columns.f_AddHeading("Identifier Scope", 0);
		Columns.f_AddHeading("Name", 0);
		Columns.f_AddHeading("Removed", 1);
		Columns.f_AddHeading("Metadata", 1);

		TableRenderer.f_AddHeadings(&Columns);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;

		CEJSONSorted JsonOutput;
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
							CEJSONSorted
							{
								"HostID"_= LogInfo.m_HostID
								, "HostName"_= LogInfo.m_HostName
								, "Application"_= Application
								, "Identifier"_= LogInfo.m_Identifier
								, "IdentifierScope"_= LogInfo.m_IdentifierScope
								, "Name"_= LogInfo.m_Name
								, "Removed"_= LogInfo.m_bRemoved
								, "LogMetaData"_= fg_MetaDataToJson(LogInfo.m_MetaData)
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
							, CHostInfo::fs_FormatFriendlyNameForTable(LogInfo.m_HostName, AnsiEncoding)
							, Application
							, LogInfo.m_Identifier
							, LogInfo.m_IdentifierScope
							, LogInfo.m_Name
							, LogInfo.m_bRemoved ? "true" : "false"
							, LogInfo.m_MetaData
						)
					;
				}
			}
		}

		if (_Flags & ELogOutputFlag_Json)
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

		auto LogsGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogs, CDistributedAppLogReader::CGetLogs{.m_Filters = {_Filter}});

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

		CTableRenderHelper::CColumnHelper Columns(_Verbosity);

		Columns.f_AddHeading("Host ID", 5);
		Columns.f_AddHeading("Host Description", 0);
		Columns.f_AddHeading("Application", 0);
		Columns.f_AddHeading("Identifier", 4);
		Columns.f_AddHeading("Identifier Scope", 5);
		Columns.f_AddHeading("Name", 0);
		Columns.f_AddHeading("Timestamp", 0);
		Columns.f_AddHeading("Sequence", 4);
		Columns.f_AddHeading("Source Location", 6);
		Columns.f_AddHeading("Categories", 0);
		Columns.f_AddHeading("Operations", 3);
		Columns.f_AddHeading("Severity", 0);
		Columns.f_AddHeading("Flags", 1);
		Columns.f_AddHeading("Message", 0);
		Columns.f_AddHeading("Metadata", 2);
		Columns.f_AddHeading("Log Metadata", 1);

		TableRenderer.f_AddHeadings(&Columns);
		TableRenderer.f_SetOptions(CTableRenderHelper::EOption_Rounded | CTableRenderHelper::EOption_AvoidRowSeparators);

		bool bHasHostID = false;
		bool bHasHostName = false;
		bool bHasApplication = false;
		bool bHasName = false;
		uint64 nEntries = 0;

		CStr TimeColor = AnsiEncoding.f_ForegroundRGB(160, 160, 160);
		CStr DebugColor = AnsiEncoding.f_ForegroundRGB(100, 100, 100);
		CStr CategoryColor = AnsiEncoding.f_ForegroundRGB(51, 182, 255);

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

		for (auto iEntries = co_await fg_Move(_LogEntries).f_GetIterator(); iEntries; co_await ++iEntries)
		{
			for (auto &Entry : *iEntries)
			{
				CStr Application;
				if (Entry.m_LogInfoKey.m_Scope.f_IsOfType<CDistributedAppLogReporter::CLogScope_Application>())
					Application = Entry.m_LogInfoKey.m_Scope.f_GetAsType<CDistributedAppLogReporter::CLogScope_Application>().m_ApplicationName;

				CStr HostName;
				CStr Name;
				NContainer::TCMap<NStr::CStr, NEncoding::CEJSONSorted> *pLogMetaData = nullptr;

				auto *pLogInfo = LogInfos.f_FindEqual(Entry.m_LogInfoKey);
				if (pLogInfo)
				{
					HostName = pLogInfo->m_HostName;
					Name = pLogInfo->m_Name;
					pLogMetaData = &pLogInfo->m_MetaData;
				}

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
							CEJSONSorted
							{
								"HostID"_= Entry.m_LogInfoKey.m_HostID
								, "HostName"_= HostName
								, "Application"_= Application
								, "Identifier"_= Entry.m_LogInfoKey.m_Identifier
								, "IdentifierScope"_= Entry.m_LogInfoKey.m_IdentifierScope
								, "LogMetaData"_= pLogMetaData ? fg_MetaDataToJson(*pLogMetaData) : CEJSONSorted()
								, "Name"_= Name
								, "Timestamp"_= Entry.m_Entry.m_Timestamp
								, "UniqueSequence"_= Entry.m_Entry.m_UniqueSequence
								, "Data"_= Data.f_ToJson()
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
							, CHostInfo::fs_FormatFriendlyNameForTable(HostName, AnsiEncoding)
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
							, pLogMetaData ? CStr("{}"_f << *pLogMetaData) : CStr()
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

			*_pCommandLine += JsonOutput.f_ToString("\t", EJSONDialectFlag_AllowUndefined | EJSONDialectFlag_AllowInvalidFloat);
		}
		else
		{
			if (!bHasName)
				Columns.f_SetVerbose("Name", 7);
			if (!bHasHostID)
				Columns.f_SetVerbose("Host ID", 7);
			if (!bHasHostName)
				Columns.f_SetVerbose("Host Description", 7);
			if (!bHasApplication)
				Columns.f_SetVerbose("Application", 7);

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
		auto EntriesGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogEntries, CDistributedAppLogReader::CGetLogEntries{.m_Filters = {_Filter}});
		auto LogsGenerator = co_await LocalStore(&CDistributedAppLogStoreLocal::f_GetLogs, CDistributedAppLogReader::CGetLogs{.m_Filters = {_Filter.m_LogFilter}});

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

