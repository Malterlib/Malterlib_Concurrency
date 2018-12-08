// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/CommandLine/AnsiEncoding>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;
	using namespace NCommandLine;

	CStr CDistributedAppCommandLineSpecification::CInternal::fs_Color(CStr const &_String, EColor _Color, bool _bColor)
	{
		if (!_bColor)
			return _String;
		return fs_Color(_String, _Color);
	}

	CStr CDistributedAppCommandLineSpecification::CInternal::fs_Color(CStr const &_String, EColor _Color)
	{
		switch (_Color)
		{
		case EColor_Executable: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0x00, 0xe4, 0xe6) << _String << CAnsiEncoding::ms_Default;
		case EColor_Command: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0x1c, 0xb9, 0x00) << _String << CAnsiEncoding::ms_Default;
		case EColor_Parameter: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xb8, 0xaa, 0xff) << _String << CAnsiEncoding::ms_Default;
		case EColor_GlobalOption: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0x77, 0x00) << _String << CAnsiEncoding::ms_Default;
		case EColor_SectionOption: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0xa6, 0x00) << _String << CAnsiEncoding::ms_Default;
		case EColor_Option: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0xd7, 0x00) << _String << CAnsiEncoding::ms_Default;
		case EColor_String: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0x00, 0x9e, 0xff) << _String << CAnsiEncoding::ms_Default;
		case EColor_Number: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0x00, 0x80) << _String << CAnsiEncoding::ms_Default;
		case EColor_Constant: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0x8a, 0xc5) << _String << CAnsiEncoding::ms_Default;
		case EColor_Date: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0x5b, 0xad) << _String << CAnsiEncoding::ms_Default;
		case EColor_Binary: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0x80, 0x80, 0x80) << _String << CAnsiEncoding::ms_Default;
		case EColor_Value: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xc0, 0xc0, 0xc0) << _String << CAnsiEncoding::ms_Default;
		case EColor_Error: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0xff, 0x3f, 0x1c) << _String << CAnsiEncoding::ms_Default;
		case EColor_Heading1: return "{}{}{}"_f << CAnsiEncoding::ms_Bold << _String << CAnsiEncoding::ms_Default;
		case EColor_Default: return "{}{}{}"_f << CAnsiEncoding::fs_ForegroundRGB(0x90, 0x90, 0x90) << _String << CAnsiEncoding::ms_Default;

		case EColor_None:
		default: return _String;
		}
	}

	TCVector<CStr> CDistributedAppCommandLineSpecification::fs_RelevantHelpGlobalOptions()
	{
		return {"HelpCurrentCommand", "HelpCurrentCommandVerbose", "Color"};
	}

	void CDistributedAppCommandLineSpecification::f_AddHelpCommand()
	{
		auto DefaultSection = f_GetDefaultSection();
		f_RegisterGlobalOptions
			(
				{
					"HelpCurrentCommand?"_=
					{
						"Names"_= {"-?"}
						, "Default"_= false
						, "Description"_= "Display help for the current command."
						, "DisablesAllErrors"_= true
					}
					, "HelpCurrentCommandVerbose?"_=
					{
						"Names"_= {"-??"}
						, "Default"_= false
						, "Description"_= "Display verbose help for the current command."
						, "DisablesAllErrors"_= true
					}
				}
			)
		;
		auto Command = DefaultSection.f_RegisterDirectCommand
			(
				{
					"Names"_= {"--help", "-h"}
					, "Description"_= "Display help."
					, "AlwaysVerbose"_= true
					, "ErrorOnCommandAsParameter"_= false
					, "ErrorOnOptionAsParameter"_= false
					, "DisableGlobalOptions"_= {"ConcurrentLogging", "StdErrLogger", "TraceLogger", "AuthenticationLifetime", "AuthenticationUser"}
					, "Parameters"_=
					{
						"Commands...?"_=
						{
							"Type"_= {""}
							, "Default"_= _[_]
							, "Description"_= "Specify the command(s) to display help for."
						}
					}
					, "Options"_=
					{
						"Verbose?"_=
						{
							"Names"_= {"--verbose", "-v"}
							, "Default"_= false
							, "Description"_= "Display the full documentation."
						}
						, "OnlyCommands?"_=
						{
							"Names"_= {"--only-commands", "-c"}
							, "Default"_= false
							, "Description"_= "Display commands only."
						}
					}
				}
				, [this](CEJSON const &_Params, CDistributedAppCommandLineClient &_CommandLineClient) -> uint32
				{
					bool bVerbose = _Params["Verbose"].f_Boolean();
					bool bColor = _CommandLineClient.f_ColorEnabled();

					auto fColor = [&](CStr const &_String, CInternal::EColor _Color)
						{
							if (bColor)
								return CInternal::fs_Color(_String, _Color);
							return _String;
						}
					;
					auto fHeading1 = [&](CStr const &_String)
						{
							return fColor(_String, CInternal::EColor_Heading1);
						}
					;
					auto fCorrectedLength = [&](CStr const &_String, mint _WantedLen) -> mint
						{
							if (!bColor)
								return _WantedLen;
							return (_String.f_GetLen() - CAnsiEncoding::fs_RenderedStrLen(_String)) + _WantedLen;
						}
					;
					auto fRenderLength = [&](CStr const &_String)
						{
							return CAnsiEncoding::fs_RenderedStrLen(_String);
						}
					;

					auto &Internal = *mp_pInternal;

					bool bFirstSection = true;

					mint Indentation = 0;

					auto ConsoleProperties = NSys::fg_GetConsoleProperties();
					mint MaxWidth = 120;
					if (ConsoleProperties.m_Width)
						MaxWidth = fg_Max(fg_Min(ConsoleProperties.m_Width, 200u), 50u); // Never wider than 200 chars, never less than 50

					mint MaxExecutableIndent = MaxWidth / 10;

					auto fHalfMaxWidth = [&]
						{
							return (MaxWidth) / 2 - Indentation;
						}
					;

					auto fLineBreak = [&](CStr _String, smint _SubsequentIndent) -> TCVector<CStr>
						{
							_String = _String.f_Replace("\t", "   ");
							smint MinAvailableWidth = 15;
							smint AvailableWidth = (smint)MaxWidth - (smint)Indentation;
							if (AvailableWidth < MinAvailableWidth)
								AvailableWidth = MinAvailableWidth;

							TCVector<CStr> Lines;
							CStr CurrentLine;
							ch8 const *pParse = _String;
							while (*pParse)
							{
								ch8 const *pStart = pParse;
								fg_ParseWhiteSpace(pParse);
								while (*pParse && !fg_CharIsWhiteSpace(*pParse))
									++pParse;
								if (!CurrentLine.f_IsEmpty() && (smint(fRenderLength(CurrentLine) + fRenderLength(CStr(pStart, pParse - pStart))) > AvailableWidth))
								{
									if (Lines.f_IsEmpty())
										AvailableWidth = fg_Max(AvailableWidth - _SubsequentIndent, MinAvailableWidth);
									Lines.f_Insert(fg_Move(CurrentLine));
									CurrentLine.f_Clear();
									fg_ParseWhiteSpace(pStart);
								}
								CurrentLine.f_AddStr(pStart, pParse - pStart);
							}

							if (!CurrentLine.f_IsEmpty())
								Lines.f_Insert(fg_Move(CurrentLine));

							return Lines;
						}
					;

					auto fAddIndent = [&](mint _Indent)
						{
							Indentation += _Indent;
							return fg_OnScopeExitShared
								(
									[&Indentation, _Indent]
									{
										Indentation -= _Indent;
									}
								)
							;
						}
					;

					bool bEndedSection = false;

					auto fOutputEndSection = [&]()
						{
							if (!bEndedSection)
							{
								DMibConOut("{\n}", 0);
								bEndedSection = true;
							}
						}
					;

					auto fOutputLine = [&](CStr _Line, bool _bEndSection = false, mint _SubsequentIndent = 0, bool _bSeparateSectionOnMultiline = true)
						{
							TCVector<CStr> Lines = fLineBreak(fg_GetStrLineSep(_Line), _SubsequentIndent);

							DMibCheck(_Line.f_IsEmpty());

							if (!Lines.f_IsEmpty())
							{
								if ((_bSeparateSectionOnMultiline && Lines.f_GetLen() > 1))
									fOutputEndSection();
								bEndedSection = false;

								COnScopeExitShared SubsequentIndentScope;
								for (auto &Line : Lines)
								{
									DMibConOut("{sj*}{}{\n}", "" << Indentation << Line.f_Replace(" ", " "));
									if (!SubsequentIndentScope)
										SubsequentIndentScope = fAddIndent(_SubsequentIndent);
								}
								if (_bEndSection || (_bSeparateSectionOnMultiline && Lines.f_GetLen() > 1))
									fOutputEndSection();
							}
						}
					;

					auto fOutputLines = [&](CStr _Lines, bool _bEndSection = false, mint _SubsequentIndent = 0)
						{
							bool bFirst = true;
							COnScopeExitShared SubsequentIndentScope;
							for (auto &Line : _Lines.f_SplitLine())
							{
								if (bFirst)
								{
									fOutputLine(Line, false, _SubsequentIndent, false);
									bFirst = false;
									SubsequentIndentScope = fAddIndent(_SubsequentIndent);
								}
								else
									fOutputLine(Line, false, 0, false);
							}
							if (!bFirst && _bEndSection)
								fOutputEndSection();
						}
					;

					auto fOutputSectionedText = [&](CStr _LongText, mint _SubsequentIndent = 0)
						{
							if (_LongText.f_IsEmpty())
								return;

							{
								COnScopeExitShared SubsequentIndentScope;
								while (!_LongText.f_IsEmpty())
								{
									smint NextSection = _LongText.f_FindChar('\n');
									smint NextLine = _LongText.f_FindChar('\r');
									bool bEndSection
										= (NextLine < 0)
										|| (NextSection >= 0 && NextLine < 0)
										|| (NextLine >= 0 && NextSection >= 0 && NextSection < NextLine)
									;

									CStr Line = fg_GetStrLineSep(_LongText);
									if (Line.f_StartsWith("@Indent="))
									{
										_SubsequentIndent = Line.f_Extract(8).f_ToInt(0);
										continue;
									}

									fOutputLine(Line, bEndSection, _SubsequentIndent);
								}
							}
							fOutputEndSection();
						}
					;

					auto fOutputTableText = [&](CStr _LongText, mint _SubsequentIndent, bool _bEndSection)
						{
							if (_LongText.f_IsEmpty())
								return;

							if (_LongText.f_FindChar('\n') >= 0)
							{
								fOutputEndSection();
								_bEndSection = true;
								_SubsequentIndent = 3;
							}

							{
								COnScopeExitShared SubsequentIndentScope;
								while (!_LongText.f_IsEmpty())
								{
									smint NextSection = _LongText.f_FindChar('\n');
									smint NextLine = _LongText.f_FindChar('\r');
									bool bEndSection
										= ((NextLine < 0) && _bEndSection)
										|| (NextSection >= 0 && NextLine < 0)
										|| (NextLine >= 0 && NextSection >= 0 && NextSection < NextLine)
									;

									CStr Line = fg_GetStrLineSep(_LongText);
									if (Line.f_StartsWith("@Indent="))
									{
										_SubsequentIndent = Line.f_Extract(8).f_ToInt(0);
										continue;
									}

									fOutputLine(Line, bEndSection, _SubsequentIndent);
								}
							}
							if (_bEndSection)
								fOutputEndSection();
						}
					;

					auto fOutputHeading = [&](CStr const &_Heading, mint _SubsequentIndent = 2)
						{
							if (fRenderLength(_Heading) == 0)
								return COnScopeExitShared();
							fOutputTableText(_Heading, _SubsequentIndent, true);
							return fAddIndent(3);
						}
					;

					auto fGetNames = [&](TCVector<CStr> const &_Names, CInternal::EColor _Color)
						{
							CStr Names;
							for (auto &Name : _Names)
								fg_AddStrSep(Names, fColor(Name, _Color), ", ");
							return Names;
						}
					;

					auto fParameterName = [&](CInternal::CParameter const &_Parameter)
						{
							CStr ParameterName = _Parameter.m_Identifier;
							if (_Parameter.m_bVector)
								ParameterName += "...";
							return ParameterName;
						}
					;

					auto fFormatTableLine = [&](CStr const &_Left, mint &_LongestLeft, CStr const &_Right) -> CStr
						{
							mint LeftWidth = fRenderLength(_Left);

							if (LeftWidth > _LongestLeft)
								return "{}\n@Indent=3\r   {}"_f << _Left << _Right;

							return "{sl*,a-}   {}"_f << _Left << fCorrectedLength(_Left, _LongestLeft) << _Right;
						}
					;

					auto fFormatShortWithDefault = [&](CStr const &_Description, CStr const &_Default) -> CStr
						{
							return "{} {}{}"_f
								<< _Description
								<< fColor("= ", CInternal::EColor_Default)
								<< _Default
							;
						}
					;

					auto fOutputOptions = [&]
						(
							CStr const &_Heading
							, TCLinkedList<CInternal::COption> const &_Options
							, CInternal::COptionSet const &_OptionSet
							, bool _bDirectCommand
							, bool _bVerbose
							, CInternal::EColor _Color
						)
						{
							if (_Options.f_IsEmpty())
								return;
							struct CCommandEntry
							{
								CStr m_Names;
								CInternal::COption const *m_pOption;
							};
							TCVector<CCommandEntry> OptionEntries;
							mint LongestName = 0;
							for (auto &Option : _Options)
							{
								if (Option.m_bHidden || !Option.f_IsEnabled(_OptionSet, _bDirectCommand))
									continue;
								CStr Names = fGetNames(Option.m_Names, _Color);
								auto &Entry = OptionEntries.f_Insert();
								LongestName = fg_Max(LongestName, mint(fRenderLength(Names)));
								Entry.m_Names = Names;
								Entry.m_pOption = &Option;
							}
							if (OptionEntries.f_IsEmpty())
								return;

							LongestName = fg_Min(LongestName, fHalfMaxWidth());

							auto Scope = fOutputHeading(_Heading);
							for (auto &OptionEntry : OptionEntries)
							{
								auto &Option = *OptionEntry.m_pOption;
								CStr Description;
								if (!_bVerbose && Option.m_Default.f_IsValid())
								{
									CStr FormattedValue = Option.f_FormatValue(Option.m_Default, bColor).f_Trim();

									if (FormattedValue.f_SplitLine().f_GetLen() > 1)
										Description = Option.m_ShortDescription;
									else
										Description = fFormatShortWithDefault(Option.m_ShortDescription, FormattedValue);
								}
								else
									Description = Option.m_ShortDescription;

								CStr OptionLine = fFormatTableLine(OptionEntry.m_Names, LongestName, Description);
								if (_bVerbose)
								{
									auto OptionScope = fOutputHeading(OptionLine, LongestName + 3);
									fOutputSectionedText(Option.m_LongDescription);
									if (Option.m_Default.f_IsValid())
									{
										CStr DefaultLine = "Default:   {}"_f << Option.f_FormatValue(Option.m_Default, bColor).f_ReplaceChar('\n', '\r');
										fOutputSectionedText(DefaultLine, 11);
									}
								}
								else
									fOutputTableText(OptionLine, LongestName + 3, false);
							}
							fOutputEndSection();
						}
					;

					auto fOutputParameters = [&](CStr const &_Heading, TCLinkedList<CInternal::CParameter> const &_Parameters, bool _bVerbose)
						{
							if (_Parameters.f_IsEmpty())
								return;

							struct CCommandEntry
							{
								CStr m_Name;
								CInternal::CParameter const *m_pParameter;
							};
							TCVector<CCommandEntry> ParameterEntries;
							mint LongestName = 0;
							for (auto &Parameter : _Parameters)
							{
								CStr Name = fColor(fParameterName(Parameter), CInternal::EColor_Parameter);

								auto &Entry = ParameterEntries.f_Insert();
								LongestName = fg_Max(LongestName, mint(fRenderLength(Name)));
								Entry.m_Name = Name;
								Entry.m_pParameter = &Parameter;
							}

							LongestName = fg_Min(LongestName, fHalfMaxWidth());

							auto Scope = fOutputHeading(_Heading);
							for (auto &ParameterEntry : ParameterEntries)
							{
								auto &Parameter = *ParameterEntry.m_pParameter;

								CStr Description;
								if (!_bVerbose && Parameter.m_Default.f_IsValid())
								{
									CStr FormattedValue = Parameter.f_FormatValue(Parameter.m_Default, bColor).f_Trim();

									if (FormattedValue.f_SplitLine().f_GetLen() > 1)
										Description = Parameter.m_ShortDescription;
									else
										Description = fFormatShortWithDefault(Parameter.m_ShortDescription, FormattedValue);
								}
								else
									Description = Parameter.m_ShortDescription;

								CStr OptionLine = fFormatTableLine(ParameterEntry.m_Name, LongestName, Description);
								fOutputTableText(OptionLine, LongestName + 3, _bVerbose);

								if (_bVerbose)
								{
									auto VerboseScope = fAddIndent(3);
									fOutputSectionedText(Parameter.m_LongDescription);
									if (Parameter.m_Default.f_IsValid())
									{
										CStr DefaultLine = fg_Format("Default:   {}", Parameter.f_FormatValue(Parameter.m_Default, bColor).f_ReplaceChar('\n', '\r'));
										fOutputSectionedText(DefaultLine, 11);
									}
								}
							}
							fOutputEndSection();
						}
					;

					struct CCommandEntry
					{
						CStr m_Text;
						CStr m_TextWithOptions;
						CStr m_Parameters;
						CStr m_Options;
						CInternal::CCommand const *m_pCommand;
					};

					auto fAddOptionUsage = [&](CStr &o_Usage, CInternal::COption const &_Option, CInternal::EColor _Color)
						{
							auto &FirstName = _Option.m_Names.f_GetFirst();
							CStr OptionString;
							if (_Option.m_TypeTemplate.f_IsBoolean())
							{
								if (_Option.m_bCanNegate)
								{
									bool bDidOption = false;
									if (FirstName.f_StartsWith("--"))
									{
										bDidOption = true;
										if (_Option.m_Default.f_IsValid())
										{
											if (_Option.m_Default.f_Boolean())
												OptionString = fColor("--no-{}"_f << FirstName.f_Extract(2), _Color);
											else
												OptionString = fColor(FirstName, _Color);
										}
										else
											OptionString = "{}[{}]{}"_f << fColor("--", _Color) << fColor("no-", _Color) << fColor(FirstName.f_Extract(2), _Color);
									}
									else if (FirstName.f_StartsWith("-"))
									{
										if (_Option.m_Default.f_IsValid())
										{
											if (_Option.m_Default.f_Boolean())
											{
												bDidOption = true;
												OptionString = "{}={}"_f << fColor(FirstName, _Color) << fColor("false", CInternal::EColor_Constant);
											}
											else
											{
												bDidOption = true;
												OptionString = fColor(FirstName, _Color);
											}
										}
									}

									if (!bDidOption)
										OptionString = "{}=({}/{})"_f << fColor(FirstName, _Color) << fColor("true", CInternal::EColor_Constant) << fColor("false", CInternal::EColor_Constant);
								}
								else
									OptionString = fColor(FirstName, _Color);
							}
							else
							{
								if (FirstName.f_StartsWith("--"))
									OptionString = fg_Format("{} {}", fColor(FirstName, _Color), fColor(_Option.m_Identifier, CInternal::EColor_Value));
								else
									OptionString = fg_Format("{}={}", fColor(FirstName, _Color), fColor(_Option.m_Identifier, CInternal::EColor_Value));
							}

							if (_Option.m_bOptional)
								fg_AddStrSep(o_Usage, CStr::CFormat("[{}]") << OptionString, " ");
							else
								fg_AddStrSep(o_Usage, OptionString, " ");
						}
					;

					auto fAddOptionsUsage = [&](CStr &o_Usage, TCLinkedList<CInternal::COption> const &_Options, CInternal::EColor _Color)
						{
							for (auto &Option : _Options)
							{
								if (Option.m_bHidden)
									continue;
								fAddOptionUsage(o_Usage, Option, _Color);
							}
						}
					;

					auto fGetCommandEntry = [&](CInternal::CCommand const &_Command) -> CCommandEntry
						{
							CStr Names = fGetNames(_Command.m_Names, CInternal::EColor_Command);
							CCommandEntry Entry;
							Entry.m_Text = Names;
							Entry.m_pCommand = &_Command;
							CStr Parameters;

							if (_Command.m_bShowOptionsInCommandEntry)
								fAddOptionsUsage(Entry.m_Options, _Command.m_Options, CInternal::EColor_Option);

							{
								TCVector<CStr> OptionalParameters;
								for (auto &Parameter : _Command.m_Parameters)
								{
									CStr ParameterName = fParameterName(Parameter);

									if (Parameter.m_bOptional)
										OptionalParameters.f_Insert(ParameterName);
									else
										fg_AddStrSep(Parameters, fColor(ParameterName, CInternal::EColor_Parameter), " ");
								}

								auto ReversedParameters = OptionalParameters.f_Reverse();

								CStr OptionalParametersFormatted;
								for (auto &Parameter : ReversedParameters)
								{
									if (OptionalParametersFormatted.f_IsEmpty())
										OptionalParametersFormatted = fg_Format("[{}]", fColor(Parameter, CInternal::EColor_Parameter));
									else
										OptionalParametersFormatted = fg_Format("[{} {}]", fColor(Parameter, CInternal::EColor_Parameter), OptionalParametersFormatted);
								}

								if (!OptionalParametersFormatted.f_IsEmpty())
									fg_AddStrSep(Parameters, OptionalParametersFormatted, " ");
							}
							Entry.m_Parameters = Parameters;
							if (!Entry.m_Options.f_IsEmpty())
								Entry.m_TextWithOptions = fg_Format("{} {} {}", Entry.m_Text, Entry.m_Options, Parameters);
							if (!Parameters.f_IsEmpty())
								Entry.m_Text = fg_Format("{} {}", Entry.m_Text, Parameters);
							if (Entry.m_Options.f_IsEmpty())
								Entry.m_TextWithOptions = Entry.m_Text;

							return Entry;
						}
					;
					auto fGetCommandUsage = [&](CCommandEntry const &_CommandEntry, mint &o_Indent)
						{
							auto &Command = *_CommandEntry.m_pCommand;
							CStr Usage;
							Usage = fColor("./" + NFile::CFile::fs_GetFileNoExt(NFile::CFile::fs_GetProgramPath()), CInternal::EColor_Executable);
							o_Indent = fg_Min(fRenderLength(Usage) + 1, MaxExecutableIndent);
							Usage += " ";
							Usage += fColor(Command.m_Names.f_GetFirst(), CInternal::EColor_Command);

							bool bAddedGlobalOption = false;
							for (auto &Option : Internal.m_GlobalOptions)
							{
								if (!Option.f_IsEnabled(Command.m_GlobalOptionSet, !!Command.m_fDirectRunCommand))
									continue;
								if (Option.m_bHidden)
									continue;

								fAddOptionUsage(Usage, Option, CInternal::EColor_GlobalOption);
								bAddedGlobalOption = true;
							}

							bool bAddedSectionOption = false;
							for (auto &Option : Command.m_pSection->m_SectionOptions)
							{
								if (!Option.f_IsEnabled(Command.m_SectionOptionSet, !!Command.m_fDirectRunCommand))
									continue;
								if (Option.m_bHidden)
									continue;

								if (bAddedGlobalOption)
								{
									Usage += "\r";
									bAddedGlobalOption = false;
								}

								bAddedSectionOption = true;
								fAddOptionUsage(Usage, Option, CInternal::EColor_SectionOption);
							}

							if (!Command.m_Options.f_IsEmpty() && (bAddedSectionOption || bAddedGlobalOption))
								Usage += "\r";

							fAddOptionsUsage(Usage, Command.m_Options, CInternal::EColor_Option);

							if (Command.m_bShowParametersStart)
								Usage += " [--]";

							if (!_CommandEntry.m_Parameters.f_IsEmpty())
							{
								Usage += " ";
								Usage += _CommandEntry.m_Parameters;
							}
							return Usage.f_Replace("\r ", "\r");
						}
					;

					auto fOutputVerboseCommand = [&](CCommandEntry const &_CommandEntry)
						{
							auto &Command = *_CommandEntry.m_pCommand;
							auto NameScope = fOutputHeading(_CommandEntry.m_Text);
							fOutputLine(Command.m_ShortDescription, true);
							fOutputSectionedText(Command.m_LongDescription);

							{
								mint Indent = 0;
								CStr Usage = fGetCommandUsage(_CommandEntry, Indent);
								auto UsageScope = fOutputHeading("Usage");
								fOutputLines(Usage, true, Indent);
							}
							if (!Command.m_Parameters.f_IsEmpty())
								fOutputParameters("Parameters", Command.m_Parameters, true);

							if (!Command.m_OutputDescription.f_IsEmpty())
							{
								auto OutputScope = fOutputHeading("Output");
								fOutputSectionedText(Command.m_OutputDescription);
							}
							if (!Command.m_StatusDescription.f_IsEmpty())
							{
								auto StatusesScope = fOutputHeading("Return Statuses");
								for (auto iStatus = Command.m_StatusDescription.f_GetIterator(); iStatus; ++iStatus)
								{
									CStr FormattedKey = fColor(iStatus.f_GetKey(), CInternal::EColor_Number);
									fOutputSectionedText("{sl*,a-}{}"_f << FormattedKey << fCorrectedLength(FormattedKey, 5) << *iStatus, 5);
								}
							}

							fOutputOptions("Options", Command.m_Options, CInternal::COptionSet(), !!Command.m_fDirectRunCommand, true, CInternal::EColor_Option);
						}
					;

					if (_Params["OnlyCommands"].f_Boolean())
					{
						mint LongestName = 0;
						TCVector<CCommandEntry> CommandEntries;
						for (auto &Section : Internal.m_Sections)
						{
							for (auto &Command : Section.m_Commands)
							{
								auto &Entry = CommandEntries.f_Insert(fGetCommandEntry(Command));
								LongestName = fg_Max(LongestName, fRenderLength(Entry.m_TextWithOptions));
							}
						}

						LongestName = fg_Min(LongestName, fHalfMaxWidth());

						fOutputEndSection();

						for (auto &CommandEntry : CommandEntries)
						{
							if (bVerbose)
								fOutputVerboseCommand(CommandEntry);
							else
							{
								CStr Line = fFormatTableLine(CommandEntry.m_TextWithOptions, LongestName, CommandEntry.m_pCommand->m_ShortDescription);
								fOutputTableText(Line, LongestName + 3, false);
							}
						}

						fOutputEndSection();

						return 0;
					}

					auto &CommandNames = _Params["Commands"].f_Array();

					if (!CommandNames.f_IsEmpty())
					{
						struct CSectionState
						{
							TCVector<CInternal::CCommand *> m_Commands;
							CInternal::COptionSet m_OptionSet;
							bool m_bIsDirectCommand = true;
						};
						TCSet<CInternal::CCommand *> CommandsMap;
						TCMap<CInternal::CSection *, CSectionState> SectionMap;
						TCVector<CInternal::CSection *> Sections;
						CInternal::COptionSet CombinedGlobalOptionsSet;
						bool bCombinedGlobalOptionsSetUsed = false;
						bool bIsDirectCommand = true;

						auto fCombineOptionSet = [&](CInternal::COptionSet &o_Destination, CInternal::COptionSet const &_ToAdd)
							{
								if (!_ToAdd.m_bAllowedSpecified)
								{
									o_Destination.m_bAllowedSpecified = false;
									o_Destination.m_Allowed.f_Clear();
								}
								else
									o_Destination.m_Allowed += _ToAdd.m_Allowed;

								o_Destination.m_Disallowed = o_Destination.m_Disallowed & _ToAdd.m_Disallowed;
							}
						;

						auto fAddCommand = [&](CInternal::CCommand *_pCommand)
							{
								if (!CommandsMap(_pCommand).f_WasCreated())
									return;
								auto &Command = *_pCommand;

								if (!bCombinedGlobalOptionsSetUsed)
									CombinedGlobalOptionsSet = Command.m_GlobalOptionSet;
								else
									fCombineOptionSet(CombinedGlobalOptionsSet, Command.m_GlobalOptionSet);

								auto MappedSection = SectionMap(Command.m_pSection);
								auto &SectionState = *MappedSection;
								if (MappedSection.f_WasCreated())
								{
									SectionState.m_OptionSet = Command.m_SectionOptionSet;
									Sections.f_Insert(Command.m_pSection);
								}
								else
									fCombineOptionSet(SectionState.m_OptionSet, Command.m_SectionOptionSet);
								SectionState.m_Commands.f_Insert(&Command);

								if (!Command.m_fDirectRunCommand)
								{
									bIsDirectCommand = false;
									SectionState.m_bIsDirectCommand = false;
								}

							}
						;
						for (auto &CommandName : CommandNames)
						{
							bool bFound = false;
							for (auto &pCommand : Internal.m_CommandByName)
							{
								CStr const &Name = Internal.m_CommandByName.fs_GetKey(pCommand);

								if (NStr::fg_StrMatchWildcard(Name.f_GetStr(), CommandName.f_String().f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
								{
									bFound = true;
									fAddCommand(pCommand);
								}
							}

							if (!bFound)
								DMibError(fg_Format("No commands found matching '{}'", CommandName.f_String()));
						}

						fOutputEndSection();

						fOutputOptions(fHeading1("Global Options"), Internal.m_GlobalOptions, CombinedGlobalOptionsSet, bIsDirectCommand, bVerbose, CInternal::EColor_GlobalOption);

						for (auto pSection : Sections)
						{
							auto &Section = *pSection;
							auto &SectionState = SectionMap[pSection];
							auto SectionScope = fOutputHeading(fHeading1(Section.m_Heading));
							if (bVerbose)
								fOutputSectionedText(Section.m_Description);

							fOutputOptions
								(
									"{} Shared Options"_f << Section.m_Heading
									, Section.m_SectionOptions
									, SectionState.m_OptionSet
									, SectionState.m_bIsDirectCommand
									, bVerbose
									, CInternal::EColor_SectionOption
								)
							;

							for (auto &pCommand : SectionState.m_Commands)
							{
								auto &Command = *pCommand;
								if (bVerbose)
									fOutputVerboseCommand(fGetCommandEntry(Command));
								else
								{
									fOutputEndSection();

									mint Indent = 0;
									CStr Usage = fGetCommandUsage(fGetCommandEntry(Command), Indent);
									fOutputLines(Usage, false, Indent);

									if (!Command.m_ShortDescription.f_IsEmpty())
									{
										fOutputEndSection();
										auto pIndent = fAddIndent(Indent);
										fOutputLine(Command.m_ShortDescription, false);
									}

									if (!Command.m_Options.f_IsEmpty())
									{
										fOutputEndSection();
										auto pIndent = fAddIndent(Indent + 3);
										fOutputOptions("", Command.m_Options, CInternal::COptionSet{}, !!Command.m_fDirectRunCommand, false, CInternal::EColor_Option);
									}

									if (!Command.m_Parameters.f_IsEmpty())
									{
										fOutputEndSection();
										auto pIndent = fAddIndent(Indent + 3);
										fOutputParameters("", Command.m_Parameters, false);
									}
								}
							}
							fOutputEndSection();
						}
						return 0;
					}

					COnScopeExitShared FirstSectionScope;

					fOutputEndSection();

					for (auto &Section : Internal.m_Sections)
					{
						auto SectionScope = fOutputHeading(fHeading1(Section.m_Heading));
						if (bVerbose)
							fOutputSectionedText(Section.m_Description);

						if (bFirstSection)
						{
							{
								auto UsageScope = fOutputHeading(fHeading1("Usage"));
								auto Executable = "./" + NFile::CFile::fs_GetFileNoExt(NFile::CFile::fs_GetProgramPath());
								fOutputLine
									(
										fg_Format
										(
											"{} [{}] [{}] [{}] [{}] [--] [{}]"
											, fColor(Executable, CInternal::EColor_Executable)
											, fColor("Command", CInternal::EColor_Command)
											, fColor("GlobalOptions", CInternal::EColor_GlobalOption)
											, fColor("SectionOptions", CInternal::EColor_SectionOption)
											, fColor("CommandOptions", CInternal::EColor_Option)
											, fColor("CommandParameters", CInternal::EColor_Parameter)
										)
										, true
										, fg_Min(fRenderLength(Executable) + 1, MaxExecutableIndent)
									)
								;
							}

							fOutputOptions(fHeading1("Global Options"), Internal.m_GlobalOptions, CInternal::COptionSet{}, false, bVerbose, CInternal::EColor_GlobalOption);
						}

						struct CCategory
						{
							CStr m_Name;
							TCVector<CCommandEntry> m_Commands;
						};

						TCMap<CStr, CCategory*> CategoryMap;
						TCLinkedList<CCategory> Categories;
						mint LongestName = 0;

						fOutputOptions("{} Shared Options"_f << Section.m_Heading, Section.m_SectionOptions, CInternal::COptionSet{}, false, bVerbose, CInternal::EColor_SectionOption);

						for (auto &Command : Section.m_Commands)
						{
							CCategory **pCategoryPointer = CategoryMap.f_FindEqual(Command.m_Category);
							CCategory *pCategory;
							if (!pCategoryPointer)
							{
								pCategory = &Categories.f_Insert();
								pCategory->m_Name = Command.m_Category;
								CategoryMap[Command.m_Category] = pCategory;
							}
							else
								pCategory = *pCategoryPointer;

							auto &Entry = pCategory->m_Commands.f_Insert(fGetCommandEntry(Command));
							LongestName = fg_Max(LongestName, fRenderLength(Entry.m_TextWithOptions));
						}

						LongestName = fg_Min(LongestName, fHalfMaxWidth());

						for (auto &Category : Categories)
						{
							auto &CategoryName = Category.m_Name;
							COnScopeExitShared CategoryScope;
							if (!CategoryName.f_IsEmpty())
								CategoryScope = fOutputHeading(CategoryName);
							for (auto &CommandEntry : Category.m_Commands)
							{
								bool bCommandVerbose = CommandEntry.m_pCommand->m_bAlwaysVerbose || bVerbose;
								if (bCommandVerbose)
									fOutputVerboseCommand(CommandEntry);
								else
								{
									CStr Line = fFormatTableLine(CommandEntry.m_TextWithOptions, LongestName, CommandEntry.m_pCommand->m_ShortDescription);
									fOutputTableText(Line, LongestName + 3, false);
								}
							}
							fOutputEndSection();
						}
						fOutputEndSection();

						if (bFirstSection)
						{
							FirstSectionScope = fg_Move(SectionScope);
							bFirstSection = false;
						}
					}
					fOutputEndSection();

					return 0;
				}
			)
		;
		f_SetDefaultCommand(Command); // Help is the default command
	}
}
