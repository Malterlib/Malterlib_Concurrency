// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/String/FuzzyMatch>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib::NConcurrency
{
	auto CDistributedAppCommandLineSpecification::f_ParseCommandLine(NContainer::TCVector<NStr::CStr> const &_Params) -> CParsedCommandLine
	{
		auto &CommandLineSpec = *mp_pInternal;

		NEncoding::CEJSON CommandParams = EJSONType_Object;

		for (auto &Option : CommandLineSpec.m_GlobalOptions)
		{
			if (Option.m_Default.f_IsValid())
				CommandParams[Option.m_Identifier] = Option.m_Default;
		}

		CDistributedAppCommandLineSpecification::CInternal::CCommand const *pFoundCommand = nullptr;
		CDistributedAppCommandLineSpecification::CInternal::CCommand const *pCurrentCommand = CommandLineSpec.m_pDefaultCommand;

		NStr::CStr FoundCommandName;

		decltype(pFoundCommand->m_Parameters.f_GetIterator()) iCommandParameter;

		bool bDefaultCommandUsed = false;

		NStr::CStr ProgramName;
		bool bParametersOnly = false;

		auto fApplyOptionDefaults = [&](CDistributedAppCommandLineSpecification::CInternal::CCommand const &_Command)
			{
				for (auto &Option : _Command.m_pSection->m_SectionOptions)
				{
					if (Option.m_Default.f_IsValid())
						CommandParams[Option.m_Identifier] = Option.m_Default;
				}

				for (auto &Option : _Command.m_Options)
				{
					if (Option.m_Default.f_IsValid())
						CommandParams[Option.m_Identifier] = Option.m_Default;
				}
			}
		;

		auto fUseDefaultCommand = [&]()
			{
				if (pFoundCommand)
					return;
				if (!bDefaultCommandUsed)
				{
					fApplyOptionDefaults(*pCurrentCommand);
					bDefaultCommandUsed = true;
				}
			}
		;

		if (pCurrentCommand)
			iCommandParameter = pCurrentCommand->m_Parameters.f_GetIterator();

		CDistributedAppCommandLineSpecification::CInternal::COption *pCurrentOption = nullptr;
		CStr CurrentOptionName;

		auto fParseValue = [&](CDistributedAppCommandLineSpecification::CInternal::CValue const &_Value, CStr const &_StringData, CStr const &_OptionName = {})
			{
				if (_OptionName.f_IsEmpty())
					CommandParams[_Value.m_Identifier] = _Value.f_ConvertValue(_StringData);
				else
					CommandParams[_Value.m_Identifier] = _Value.f_ConvertValue(_StringData, _OptionName);
			}
		;

		bool bUsedVectorParam = false;

		for (auto &Parameter : _Params)
		{
			if (ProgramName.f_IsEmpty())
			{
				ProgramName = Parameter;
				continue;
			}

			if (Parameter == "--")
			{
				bParametersOnly = true;
				continue;
			}

			if (!bParametersOnly)
			{
				if (pCurrentOption)
				{
					fParseValue(*pCurrentOption, Parameter, CurrentOptionName);
					pCurrentOption = nullptr;
					CurrentOptionName.f_Clear();
					continue;
				}

				CStr ParsedParameter = Parameter;
				CStr OptionValue;
				bool bOptionValueSet = false;
				bool bNegatedOption = false;

				if (Parameter.f_StartsWith("--no-"))
				{
					ParsedParameter = "--" + Parameter.f_Extract(5);
					bOptionValueSet = true;
					OptionValue = "false";
					bNegatedOption = true;
				}
				else
				{
					aint iParameterAssign = Parameter.f_FindChar('=');
					if (iParameterAssign >= 0)
					{
						OptionValue = Parameter.f_Extract(iParameterAssign + 1);
						bOptionValueSet = true;
						ParsedParameter = Parameter.f_Left(iParameterAssign);
					}
				}

				auto *pCommand = CommandLineSpec.m_CommandByName.f_FindEqual(ParsedParameter);
				if (pCommand)
				{
					if (bOptionValueSet)
						DMibError(fg_Format("You cannot specify a command parameter with '=': {}", OptionValue));
					if (pFoundCommand)
					{
						if (pFoundCommand->m_bErrorOnCommandAsParameter)
							DMibError(fg_Format("Command '{}' already specified. You cannot specify additional command '{}'", FoundCommandName, ParsedParameter));
					}
					else
					{
						if (bDefaultCommandUsed)
						{
							DMibError
								(
									fg_Format
									(
										"Options have already been parsed for the default command. You should specify the command '{}' first on the command line"
										, ParsedParameter
									)
								)
							;
						}

						pCurrentCommand = pFoundCommand = *pCommand;
						FoundCommandName = ParsedParameter;
						iCommandParameter = pFoundCommand->m_Parameters.f_GetIterator();

						fApplyOptionDefaults(*pFoundCommand);

						continue;
					}
				}

				auto fParseOption = [&](CDistributedAppCommandLineSpecification::CInternal::COption const &_Value, CStr const &_OptionName)
					{
						if (_Value.m_TypeTemplate.f_IsBoolean())
						{
							if (!bOptionValueSet)
							{
								OptionValue = "true";
								bOptionValueSet = true;
							}
						}
						else if (bNegatedOption)
							DMibError(fg_Format("You cannot negate a non-boolean option ({}) with --no-", _Value.m_Names.f_GetFirst()));
						if (bOptionValueSet)
						{
							fParseValue(_Value, OptionValue, _OptionName);
							return true;
						}
						return false;
					}
				;

				if (pCurrentCommand)
				{
					if (auto *pOption = pCurrentCommand->m_OptionsByName.f_FindEqual(ParsedParameter))
					{
						fUseDefaultCommand();
						if (fParseOption(**pOption, ParsedParameter))
							continue;
						pCurrentOption = *pOption;
						CurrentOptionName = ParsedParameter;
						continue;
					}
					if (auto *pOption = pCurrentCommand->m_pSection->m_SectionOptionsByName.f_FindEqual(ParsedParameter))
					{
						if
							(
								!(*pOption)->m_bDefaultEnabled
								&& !pCurrentCommand->m_AllowedSectionOptions.f_IsEmpty()
								&& !pCurrentCommand->m_AllowedSectionOptions.f_FindEqual((*pOption)->m_Identifier)
							)
						{
							DMibError(fg_Format("Option '{}' is not allowed for command '{}'", (*pOption)->m_Names.f_GetFirst(), pCurrentCommand->m_Names.f_GetFirst()));
						}
						if (pCurrentCommand->m_DisallowedSectionOptions.f_FindEqual((*pOption)->m_Identifier))
							DMibError(fg_Format("Option '{}' is not allowed for command '{}'", (*pOption)->m_Names.f_GetFirst(), pCurrentCommand->m_Names.f_GetFirst()));

						fUseDefaultCommand();
						if (fParseOption(**pOption, ParsedParameter))
							continue;
						pCurrentOption = *pOption;
						CurrentOptionName = ParsedParameter;
						continue;
					}
				}

				if (auto *pOption = CommandLineSpec.m_GlobalOptionsByName.f_FindEqual(ParsedParameter))
				{
					if (fParseOption(**pOption, ParsedParameter))
						continue;
					pCurrentOption = *pOption;
					CurrentOptionName = ParsedParameter;
					continue;
				}

				if (Parameter.f_StartsWith("-") && (!pFoundCommand || pFoundCommand->m_bErrorOnOptionAsParameter))
				{
					struct CFuzzyEntry
					{
						CStr m_Name;
						fp64 m_Score;
						bool operator < (CFuzzyEntry const &_Right) const
						{
							return fg_TupleReferences(m_Score, m_Name) < fg_TupleReferences(_Right.m_Score, _Right.m_Name);
						}
					};

					TCVector<CFuzzyEntry> FuzzyEntries;

					auto fCheckName = [&](CStr const &_Name)
						{
							fp64 Score = NStr::fg_FuzzyMatchString(_Name, Parameter);
							auto &Entry = FuzzyEntries.f_Insert();
							Entry.m_Name = _Name;
							Entry.m_Score = Score;
						}
					;

					for (auto iName = CommandLineSpec.m_CommandByName.f_GetIterator(); iName; ++iName)
						fCheckName(iName.f_GetKey());

					if (pCurrentCommand)
					{
						for (auto iName = pCurrentCommand->m_OptionsByName.f_GetIterator(); iName; ++iName)
							fCheckName(iName.f_GetKey());
					}

					for (auto &Option : CommandLineSpec.m_GlobalOptions)
					{
						for (auto &Name : Option.m_Names)
							fCheckName(Name);
					}

					FuzzyEntries.f_Sort();

					CStr Error = fg_Format("No such option or command '{}'", ParsedParameter);
					if (!FuzzyEntries.f_IsEmpty())
					{
						CStr EntryNames;
						fp64 BestScore = FuzzyEntries.f_GetFirst().m_Score;
						for (auto &Entry : FuzzyEntries)
						{
#if 1
							if ((Entry.m_Score - BestScore) > 0.2)
								break;
							fg_AddStrSep(EntryNames, fg_Format("   {}", Entry.m_Name) ,"\n");
#else
							if ((Entry.m_Score - BestScore) > 0.2)
								fg_AddStrSep(EntryNames, fg_Format("-- {fe2} {}", Entry.m_Score, Entry.m_Name) ,"\n");
							else
								fg_AddStrSep(EntryNames, fg_Format("   {fe2} {}", Entry.m_Score, Entry.m_Name) ,"\n");
#endif
						}
						if (!EntryNames.f_IsEmpty())
							Error += ". Did you mean any of the following?\n{}"_f << EntryNames;
					}

					DMibError(Error);
				}
			}

			if (!iCommandParameter)
				DMibError(fg_Format("Unexpected parameter: {}", Parameter));

			fUseDefaultCommand();
			if (iCommandParameter->m_bVector)
			{
				bUsedVectorParam = true;
				iCommandParameter->f_AppendConvertValue(CommandParams[iCommandParameter->m_Identifier], Parameter);
			}
			else
			{
				fParseValue(*iCommandParameter, Parameter);
				++iCommandParameter;
			}
		}

		fUseDefaultCommand();

		if (pCurrentOption)
			DMibError(fg_Format("Missing parameter for option: {}", CurrentOptionName));

		if (!pFoundCommand)
			pFoundCommand = CommandLineSpec.m_pDefaultCommand;

		if (!pFoundCommand)
			DMibError("No command specified");

		auto fCheckOption = [&](CDistributedAppCommandLineSpecification::CInternal::COption const &_Option)
			{
				if (!_Option.m_bOptional && !CommandParams.f_GetMember(_Option.m_Identifier))
				{
					DMibError(fg_Format("Missing required option: {}", _Option.m_Names.f_GetFirst()));
				}
			}
		;
		for (auto &Option : CommandLineSpec.m_GlobalOptions)
			fCheckOption(Option);
		for (auto &Option : pFoundCommand->m_pSection->m_SectionOptions)
			fCheckOption(Option);
		for (auto &Option : pFoundCommand->m_Options)
			fCheckOption(Option);

		if (iCommandParameter && !iCommandParameter->m_bOptional && !bUsedVectorParam)
			DMibError(fg_Format("Missing required command parameter: {}", iCommandParameter->m_Identifier));

		for (; iCommandParameter; ++iCommandParameter)
		{
			if (iCommandParameter->m_bVector && bUsedVectorParam)
				break;
			if (iCommandParameter->m_Default.f_IsValid())
				CommandParams[iCommandParameter->m_Identifier] = iCommandParameter->m_Default;
		}

		CommandParams["Command"] = pFoundCommand->m_Names.f_GetFirst();

		return {pFoundCommand->m_Names.f_GetFirst(), fg_Move(CommandParams)};
	}
}
