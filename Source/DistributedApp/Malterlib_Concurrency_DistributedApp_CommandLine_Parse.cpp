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

		bool bDisableAllErrors = false;
		NStr::CStr FoundCommandName;

		decltype(pFoundCommand->m_Parameters.f_GetIterator()) iCommandParameter;

		bool bDefaultCommandUsed = false;

		NStr::CStr ProgramName;
		bool bParametersOnly = false;

		bool bColor = true;
		auto fColor = [&](CStr const &_String, CInternal::EColor _Color) -> CStr
			{
				if (bColor)
					return CInternal::fs_Color(_String, _Color);
				return _String;
			}
		;
		auto fColorValue = [&](CInternal::EColor _Color) -> CInternal::EColor
			{
				if (bColor)
					return _Color;
				return CInternal::EColor_None;
			}
		;

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

		auto fFoundCommand = [&](CDistributedAppCommandLineSpecification::CInternal::CCommand const *_pCommand, CStr const &_CommandName)
			{
				pCurrentCommand = pFoundCommand = _pCommand;
				FoundCommandName = _CommandName;
				iCommandParameter = pFoundCommand->m_Parameters.f_GetIterator();

				fApplyOptionDefaults(*pFoundCommand);
			}
		;

		if (pCurrentCommand && pCurrentCommand->m_bGreedyDefaultCommand)
			fFoundCommand(pCurrentCommand, pCurrentCommand->m_Names.f_GetFirst());
		else if (pCurrentCommand)
			iCommandParameter = pCurrentCommand->m_Parameters.f_GetIterator();

		CDistributedAppCommandLineSpecification::CInternal::COption *pCurrentOption = nullptr;
		CStr CurrentOptionName;
		CInternal::EColor CurrentOptionColor = CInternal::EColor_Option;

		auto fParseValue = [&](CDistributedAppCommandLineSpecification::CInternal::CValue const &_Value, CStr const &_StringData, CInternal::EColor _Color, CStr const &_OptionName = {})
			{
				if (_OptionName.f_IsEmpty())
					CommandParams[_Value.m_Identifier] = _Value.f_ConvertValue(_StringData, fColorValue(_Color));
				else
					CommandParams[_Value.m_Identifier] = _Value.f_ConvertValue(_StringData, _OptionName, fColorValue(_Color));
			}
		;

		bool bUsedVectorParam = false;
		bool bFatalError = false;

		NContainer::TCVector<NException::CException> Exceptions;

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

			try
			{
				if (!bParametersOnly)
				{
					if (pCurrentOption)
					{
						fParseValue(*pCurrentOption, Parameter, CurrentOptionColor, CurrentOptionName);
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
							{
								DMibError
									(
										"Command {} already specified. You cannot specify additional command {}"_f
										<< fColor(FoundCommandName, CInternal::EColor_Command)
										<< fColor(ParsedParameter, CInternal::EColor_Command)
									)
								;
							}
						}
						else
						{
							if (bDefaultCommandUsed)
							{
								DMibError
									(
										fg_Format
										(
											"Options have already been parsed for the default command. You should specify {} first on the command line"
											, fColor(ParsedParameter, CInternal::EColor_Command)
										)
									)
								;
							}

							fFoundCommand(*pCommand, ParsedParameter);
							continue;
						}
					}

					auto fParseOption = [&](CDistributedAppCommandLineSpecification::CInternal::COption const &_Value, CStr const &_OptionName, CInternal::EColor _Color)
						{
							if (_Value.m_TypeTemplate.f_IsBoolean())
							{
								if (bNegatedOption && !_Value.m_bCanNegate)
									DMibError(fg_Format("Option {} cannot be negated with --no-", fColor(_Value.m_Names.f_GetFirst(), _Color)));

								if (!bOptionValueSet)
								{
									OptionValue = "true";
									bOptionValueSet = true;
								}
							}
							else if (bNegatedOption)
								DMibError(fg_Format("You cannot negate a non-boolean option {} with --no-", fColor(_Value.m_Names.f_GetFirst(), _Color)));

							if (_Value.m_bDisablesAllErrors)
								bDisableAllErrors = true;

							if (bOptionValueSet)
							{
								fParseValue(_Value, OptionValue, CurrentOptionColor, _OptionName);
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
							if (fParseOption(**pOption, ParsedParameter, CInternal::EColor_Option))
								continue;
							pCurrentOption = *pOption;
							CurrentOptionName = ParsedParameter;
							CurrentOptionColor = CInternal::EColor_Option;
							continue;
						}
						if (auto *pOption = pCurrentCommand->m_pSection->m_SectionOptionsByName.f_FindEqual(ParsedParameter))
						{
							if (!(*pOption)->f_IsEnabled(pCurrentCommand->m_SectionOptionSet, !!pCurrentCommand->m_fDirectRunCommand))
							{
								DMibError
									(
										"Option {} is not allowed for command {}"_f
										<< fColor((*pOption)->m_Names.f_GetFirst(), CInternal::EColor_SectionOption)
										<< fColor(pCurrentCommand->m_Names.f_GetFirst(), CInternal::EColor_Command)
									)
								;
							}

							fUseDefaultCommand();
							if (fParseOption(**pOption, ParsedParameter, CInternal::EColor_SectionOption))
								continue;
							pCurrentOption = *pOption;
							CurrentOptionName = ParsedParameter;
							CurrentOptionColor = CInternal::EColor_SectionOption;
							continue;
						}
					}

					if (auto *pOption = CommandLineSpec.m_GlobalOptionsByName.f_FindEqual(ParsedParameter))
					{
						if (pCurrentCommand)
						{
							if (!(*pOption)->f_IsEnabled(pCurrentCommand->m_GlobalOptionSet, !!pCurrentCommand->m_fDirectRunCommand))
							{
								DMibError
									(
										"Option {} is not allowed for command {}"_f
										<< fColor((*pOption)->m_Names.f_GetFirst(), CInternal::EColor_GlobalOption)
										<< fColor(pCurrentCommand->m_Names.f_GetFirst(), CInternal::EColor_Command)
									)
								;
							}
						}

						if (fParseOption(**pOption, ParsedParameter, CInternal::EColor_GlobalOption))
							continue;
						pCurrentOption = *pOption;
						CurrentOptionName = ParsedParameter;
						CurrentOptionColor = CInternal::EColor_GlobalOption;
						continue;
					}

					if
						(
							(Parameter.f_StartsWith("-") && (!pFoundCommand || pFoundCommand->m_bErrorOnOptionAsParameter))
							|| (!Parameter.f_StartsWith("-") && !pFoundCommand)
						)
					{
						struct CFuzzyEntry
						{
							CStr m_Name;
							CStr m_ColoredName;
							fp64 m_Score;
							bool operator < (CFuzzyEntry const &_Right) const
							{
								return fg_TupleReferences(m_Score, m_Name) < fg_TupleReferences(_Right.m_Score, _Right.m_Name);
							}
						};

						TCVector<CFuzzyEntry> FuzzyEntries;

						auto fCheckName = [&](CStr const &_Name, CInternal::EColor _Color)
							{
								fp64 Score = NStr::fg_FuzzyMatchString(_Name, Parameter);
								auto &Entry = FuzzyEntries.f_Insert();
								Entry.m_Name = _Name;
								Entry.m_Score = Score;
								Entry.m_ColoredName = fColor(_Name, _Color);
							}
						;

						if (!pFoundCommand)
						{
							bFatalError = true;
							for (auto iName = CommandLineSpec.m_CommandByName.f_GetIterator(); iName; ++iName)
								fCheckName(iName.f_GetKey(), CInternal::EColor_Command);
						}

						if (pCurrentCommand)
						{
							for (auto iName = pCurrentCommand->m_OptionsByName.f_GetIterator(); iName; ++iName)
								fCheckName(iName.f_GetKey(), CInternal::EColor_Option);

							for (auto iName = pCurrentCommand->m_pSection->m_SectionOptionsByName.f_GetIterator(); iName; ++iName)
								fCheckName(iName.f_GetKey(), CInternal::EColor_SectionOption);
						}

						for (auto &Option : CommandLineSpec.m_GlobalOptions)
						{
							for (auto &Name : Option.m_Names)
								fCheckName(Name, CInternal::EColor_GlobalOption);
						}

						FuzzyEntries.f_Sort();

						CStr Error = fg_Format("No such option or command: {}", fColor(ParsedParameter, CInternal::EColor_Error));
						if (!FuzzyEntries.f_IsEmpty())
						{
							CStr EntryNames;
							fp64 BestScore = FuzzyEntries.f_GetFirst().m_Score;
							for (auto &Entry : FuzzyEntries)
							{
	#if 1
								if ((Entry.m_Score - BestScore) > 0.2)
									break;
								fg_AddStrSep(EntryNames, fg_Format("   {}", Entry.m_ColoredName) ,"\n");
	#else
								if ((Entry.m_Score - BestScore) > 0.2)
									fg_AddStrSep(EntryNames, fg_Format("-- {fe2} {}", Entry.m_Score, Entry.m_ColoredName) ,"\n");
								else
									fg_AddStrSep(EntryNames, fg_Format("   {fe2} {}", Entry.m_Score, Entry.m_ColoredName) ,"\n");
	#endif
							}
							if (!EntryNames.f_IsEmpty())
								Error += ". Did you mean any of the following?\n{}"_f << EntryNames;
						}

						DMibError(Error);
					}
				}

				if (!iCommandParameter)
					DMibError(fg_Format("Unexpected parameter value: {}", fColor(Parameter, CInternal::EColor_Error)));

				fUseDefaultCommand();
				if (iCommandParameter->m_bVector)
				{
					bUsedVectorParam = true;
					iCommandParameter->f_AppendConvertValue(CommandParams[iCommandParameter->m_Identifier], Parameter, fColorValue(CInternal::EColor_Parameter));
				}
				else
				{
					fParseValue(*iCommandParameter, Parameter, CInternal::EColor_Parameter);
					++iCommandParameter;
				}
			}
			catch (NException::CException const &_Exception)
			{
				Exceptions.f_Insert(_Exception);
				if (!bParametersOnly)
				{
					pCurrentOption = nullptr;
					CurrentOptionName.f_Clear();
				}
				if (bFatalError)
					break;
			}
		}

		fUseDefaultCommand();

		bool bFoundCommand = !!pFoundCommand;

		if (!pFoundCommand)
			pFoundCommand = CommandLineSpec.m_pDefaultCommand;

		if (!pFoundCommand)
			Exceptions.f_Insert(DMibErrorInstance("No command specified"));

		if (pCurrentOption)
			Exceptions.f_Insert(DMibErrorInstance("Missing parameter for option: {}"_f << fColor(CurrentOptionName, CurrentOptionColor)));

		auto fCheckOption = [&](CDistributedAppCommandLineSpecification::CInternal::COption const &_Option, CInternal::EColor _Color)
			{
				if (!_Option.m_bOptional && !CommandParams.f_GetMember(_Option.m_Identifier))
					Exceptions.f_Insert(DMibErrorInstance("Missing required option: {}"_f << fColor(_Option.m_Names.f_GetFirst(), _Color)));
			}
		;
		for (auto &Option : CommandLineSpec.m_GlobalOptions)
			fCheckOption(Option, CInternal::EColor_GlobalOption);
		for (auto &Option : pFoundCommand->m_pSection->m_SectionOptions)
			fCheckOption(Option, CInternal::EColor_SectionOption);
		for (auto &Option : pFoundCommand->m_Options)
			fCheckOption(Option, CInternal::EColor_Option);

		CStr MissingParameters;
		while (iCommandParameter && !iCommandParameter->m_bOptional && !bUsedVectorParam)
		{
			fg_AddStrSep(MissingParameters, fColor(iCommandParameter->m_Identifier, CInternal::EColor_Parameter), " ");
			++iCommandParameter;
		}

		if (!MissingParameters.f_IsEmpty())
			Exceptions.f_Insert(DMibErrorInstance("Missing required command parameters: {}"_f << MissingParameters));

		for (; iCommandParameter; ++iCommandParameter)
		{
			if (iCommandParameter->m_bVector && bUsedVectorParam)
				break;
			if (iCommandParameter->m_Default.f_IsValid())
				CommandParams[iCommandParameter->m_Identifier] = iCommandParameter->m_Default;
		}

		if (!Exceptions.f_IsEmpty() && (!pFoundCommand || !bDisableAllErrors))
		{
			NStr::CStr CombinedException;
			bool bWasMultiLine = false;

			for (auto &Exception : Exceptions)
			{
				auto ExceptionStr = Exception.f_GetErrorStr();
				bool bMultiLine = ExceptionStr.f_SplitLine().f_GetLen() > 1;

				if (bMultiLine || bWasMultiLine)
					fg_AddStrSep(CombinedException, Exception.f_GetErrorStr(), "\n\n");
				else
					fg_AddStrSep(CombinedException, Exception.f_GetErrorStr(), "\n");

				bWasMultiLine = bMultiLine;
			}

			if (bFoundCommand)
				CombinedException += "\n\nTo see command syntax, run command with {}\n"_f << fColor("-?", CInternal::EColor_GlobalOption);

			DMibError(CombinedException);
		}

		CommandParams["Command"] = pFoundCommand->m_Names.f_GetFirst();

		return {pFoundCommand->m_Names.f_GetFirst(), fg_Move(CommandParams)};
	}
}
