// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Encoding/JSONShortcuts>

#include "Malterlib_Concurrency_DistributedApp.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib
{
	using namespace NStr;
	using namespace NFile;
	using namespace NPtr;
	using namespace NEncoding;
	using namespace NContainer;
	
	namespace NConcurrency
	{
		void CDistributedAppCommandLineSpecification::fp_SetupHelpCommand()
		{
			auto DefaultSection = f_GetDefaultSection();
			auto Command = DefaultSection.f_RegisterDirectCommand
				(
					{
						"Names"_= {"--help", "-h"}
						, "Description"_= "Display help."
						, "AlwaysVerbose"_= true
						, "ErrorOnCommandAsParameter"_= false
						, "ErrorOnOptionAsParameter"_= false
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
						
						auto &Internal = *mp_pInternal;
						
						bool bFirstSection = true;
						
						mint Indentation = 0;
						
						auto ConsoleProperties = NSys::fg_GetConsoleProperties();
						mint MaxWidth = 120;
						if (ConsoleProperties.m_Width)
							MaxWidth = fg_Max(fg_Min(ConsoleProperties.m_Width, 200u), 80u); // Never wider than 200 chars, never less than 80
						
						auto fLineBreak = [&](CUStr _String, smint _SubsequentIndent) -> TCVector<CStr>
							{
								_String = _String.f_Replace("\t", "   ");
								smint MinAvailableWidth = 15;
								smint AvailableWidth = (smint)MaxWidth - (smint)Indentation;
								if (AvailableWidth < MinAvailableWidth)
									AvailableWidth = MinAvailableWidth;
								
								TCVector<CStr> Lines;
								CUStr CurrentLine;
								ch32 const *pParse = _String;
								while (*pParse)
								{
									ch32 const *pStart = pParse;
									fg_ParseWhiteSpace(pParse);
									while (*pParse && !fg_CharIsWhiteSpace(*pParse))
										++pParse;
									if (!CurrentLine.f_IsEmpty() && (CurrentLine.f_GetLen() + (pParse - pStart) > AvailableWidth))
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
						
						auto fOutputLine = [&](CStr _Line, bool _bEndSection = false, mint _SubsequentIndent = 0)
							{
								TCVector<CStr> Lines = fLineBreak(fg_GetStrLineSep(_Line), _SubsequentIndent);
								
								DMibCheck(_Line.f_IsEmpty());

								bEndedSection = false;
								
								if (!Lines.f_IsEmpty())
								{
									COnScopeExitShared SubsequentIndentScope; 
									for (auto &Line : Lines)
									{
										DMibConOut("{sj*}{}{\n}", "" << Indentation << Line);
										if (!SubsequentIndentScope)
											SubsequentIndentScope = fAddIndent(_SubsequentIndent);
									}
									if (_bEndSection)
										fOutputEndSection();
								}
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
										bool bEndSection = (NextLine < 0) || (NextSection >= 0 && NextLine < 0) || (NextLine >= 0 && NextSection >= 0 && NextSection < NextLine);
										
										CStr Line = fg_GetStrLineSep(_LongText);
										if (Line.f_StartsWith("@Indent="))
										{
											_SubsequentIndent = Line.f_Extract(8).f_ToInt(0);
											continue;
										}
										
										fOutputLine(Line, bEndSection, _SubsequentIndent);
										if (!SubsequentIndentScope)
											SubsequentIndentScope = fAddIndent(_SubsequentIndent);
									}
								}
								fOutputEndSection();
							}
						;
						
						auto fOutputHeading = [&](CStr const &_Heading, mint _SubsequentIndent = 2)
							{
								if (_Heading.f_IsEmpty())
									return COnScopeExitShared();
								fOutputLine(_Heading, true, _SubsequentIndent);
								return fAddIndent(3);
							}
						;
						
						auto fGetNames = [](TCVector<CStr> const &_Names)
							{
								CStr Names;
								for (auto &Name : _Names)
									fg_AddStrSep(Names, Name, ", ");
								return Names;
							}
						;
						
						auto fOutputOptions = [&](CStr const &_Heading, TCLinkedList<CInternal::COption> const &_Options, bool _bVerbose)
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
									if (Option.m_bHidden)
										continue;
									CStr Names = fGetNames(Option.m_Names);
									auto &Entry = OptionEntries.f_Insert();
									LongestName = fg_Max(LongestName, mint(Names.f_GetLen()));
									Entry.m_Names = Names;
									Entry.m_pOption = &Option;
								}
								auto Scope = fOutputHeading(_Heading);
								for (auto &OptionEntry : OptionEntries)
								{
									auto &Option = *OptionEntry.m_pOption;
									auto OptionLine = fg_Format("{sl*,a-}   {}", OptionEntry.m_Names, LongestName, Option.m_ShortDescription);
									if (_bVerbose)
									{
										auto OptionScope = fOutputHeading(OptionLine, LongestName + 5);
										fOutputSectionedText(Option.m_LongDescription);
										if (Option.m_Default.f_IsValid())
										{
											CStr DefaultLine = fg_Format("Default:   {}", Option.f_FormatValue(Option.m_Default).f_ReplaceChar('\n', '\r'));
											fOutputSectionedText(DefaultLine, 11);
										}
									}
									else
										fOutputLine(OptionLine);
								}
								fOutputEndSection();
							}
						;
						
						struct CCommandEntry
						{
							CStr m_Names;
							CStr m_Parameters;
							CInternal::CCommand const *m_pCommand;
						};

						auto fGetCommandEntry = [&](CInternal::CCommand const &_Command) -> CCommandEntry
							{
								CStr Names = fGetNames(_Command.m_Names);
								CCommandEntry Entry;
								Entry.m_Names = Names;
								Entry.m_pCommand = &_Command;
								CStr Parameters;
								{
									TCVector<CStr> OptionalParameters;
									for (auto &Parameter : _Command.m_Parameters)
									{
										CStr ParameterName = Parameter.m_Identifier;
										if (Parameter.m_bVector)
											ParameterName += " ...";
										if (Parameter.m_bOptional)
											OptionalParameters.f_Insert(ParameterName);
										else
											fg_AddStrSep(Parameters, ParameterName, " ");
									}
									
									auto ReversedParameters = OptionalParameters.f_Reverse();
									
									CStr OptionalParametersFormatted;
									for (auto &Parameter : ReversedParameters)
									{
										if (OptionalParametersFormatted.f_IsEmpty())
											OptionalParametersFormatted = fg_Format("[{}]", Parameter);
										else
											OptionalParametersFormatted = fg_Format("[{} {}]", Parameter, OptionalParametersFormatted);
									}
									
									if (!OptionalParametersFormatted.f_IsEmpty())
										fg_AddStrSep(Parameters, OptionalParametersFormatted, " ");
								}
								Entry.m_Parameters = Parameters;
								if (!Parameters.f_IsEmpty())
									Entry.m_Names = fg_Format("{} {}", Entry.m_Names, Parameters);
								
								return Entry;
							}
						;

						auto fAddOptionUsage = [&](CStr &o_Usage, CInternal::COption const &_Option)
							{
								auto &FirstName = _Option.m_Names.f_GetFirst();
								CStr OptionString;
								if (_Option.m_TypeTemplate.f_IsBoolean())
								{
									if (FirstName.f_StartsWith("--"))
										OptionString = fg_Format("--[no-]{}", FirstName.f_Extract(2));
									else
										OptionString = fg_Format("{}=(true/false)", FirstName);
								}
								else
								{
									OptionString = fg_Format("{} {}", FirstName, _Option.m_Identifier);
								}
									
								if (_Option.m_bOptional)
									fg_AddStrSep(o_Usage, CStr::CFormat("[{}]") << OptionString, " ");
								else
									fg_AddStrSep(o_Usage, OptionString, " ");
							}
						;
						
						auto fAddOptionsUsage = [&](CStr &o_Usage, TCLinkedList<CInternal::COption> const &_Options)
							{
								for (auto &Option : _Options)
								{
									if (Option.m_bHidden)
										continue;
									fAddOptionUsage(o_Usage, Option);
								}
							}
						;
						
						auto fGetCommandUsage = [&](CCommandEntry const &_CommandEntry)
							{
								auto &Command = *_CommandEntry.m_pCommand;
								CStr Usage;
								Usage = NFile::CFile::fs_GetFileNoExt(NFile::CFile::fs_GetProgramPath());
								Usage += " "; 
								Usage += Command.m_Names.f_GetFirst(); 
								
								fAddOptionsUsage(Usage, Internal.m_GlobalOptions);
								
								for (auto &Option : Command.m_pSection->m_SectionOptions)
								{
									if (Command.m_DisallowedSectionOptions.f_FindEqual(Option.m_Identifier))
										continue;
									if (!Option.m_bDefaultEnabled && !Command.m_AllowedSectionOptions.f_FindEqual(Option.m_Identifier))
										continue;
									if (Option.m_bHidden)
										continue;
									
									fAddOptionUsage(Usage, Option);
								}

								fAddOptionsUsage(Usage, Command.m_Options);

								Usage += " [--]";
								
								if (!_CommandEntry.m_Parameters.f_IsEmpty())
								{
									Usage += " ";
									Usage += _CommandEntry.m_Parameters;
								}
								return Usage;
							}
						;
						
						auto fOutputVerboseCommand = [&](CCommandEntry const &_CommandEntry)
							{
								auto &Command = *_CommandEntry.m_pCommand;
								auto NameScope = fOutputHeading(_CommandEntry.m_Names);
								fOutputLine(Command.m_ShortDescription, true);
								fOutputSectionedText(Command.m_LongDescription);
								
								{
									CStr Usage = fGetCommandUsage(_CommandEntry);
									auto UsageScope = fOutputHeading("Usage");
									fOutputLine(Usage, true, 2);
								}
								if (!Command.m_Parameters.f_IsEmpty())
								{
									auto ParametersScope = fOutputHeading("Parameters");
									for (auto &Parameter : Command.m_Parameters)
									{
										auto ParameterScope = fOutputHeading(Parameter.m_Identifier);
										fOutputLine(Parameter.m_ShortDescription, true);
										fOutputSectionedText(Parameter.m_LongDescription);
										
										if (Parameter.m_Default.f_IsValid())
										{
											CStr DefaultLine = fg_Format("Default:   {}", Parameter.m_Default.f_ToString().f_ReplaceChar('\n', '\r'));
											fOutputSectionedText(DefaultLine, 11);
										}
									}								
								}
								if (!Command.m_OutputDescription.f_IsEmpty())
								{
									auto OutputScope = fOutputHeading("Output");
									fOutputSectionedText(Command.m_OutputDescription);
								}
								if (!Command.m_StatusDescription.f_IsEmpty())
								{
									auto StatusesScope = fOutputHeading("Return Statuses");
									for (auto iStatus = Command.m_StatusDescription.f_GetIterator(); iStatus; ++iStatus)
										fOutputSectionedText(fg_Format("{sl5,a-}{}", iStatus.f_GetKey(), *iStatus), 5);
								}
								
								fOutputOptions("Options", Command.m_Options, true);
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
									LongestName = fg_Max(aint(LongestName), Entry.m_Names.f_GetLen());
								}
							}
							for (auto &CommandEntry : CommandEntries)
							{
								if (bVerbose)
									fOutputVerboseCommand(CommandEntry);
								else
									fOutputLine(fg_Format("{sl*,a-}   {}", CommandEntry.m_Names, LongestName, CommandEntry.m_pCommand->m_ShortDescription), false, LongestName + 5);
							}
						
							return 0;
						}
						
						auto &CommandNames = _Params["Commands"].f_Array();
						
						if (!CommandNames.f_IsEmpty())
						{
							TCSet<CInternal::CCommand *> CommandsMap;
							TCMap<CInternal::CSection *, TCVector<CInternal::CCommand *>> SectionMap;
							TCVector<CInternal::CSection *> Sections;
							
							auto fAddCommand = [&](CInternal::CCommand *_pCommand)
								{
									if (!CommandsMap(_pCommand).f_WasCreated())
										return;
									auto &Command = *_pCommand;
									auto MappedSection = SectionMap(Command.m_pSection);
									if (MappedSection.f_WasCreated())
										Sections.f_Insert(Command.m_pSection);
									(*MappedSection).f_Insert(&Command);
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
							fOutputOptions("Global Options", Internal.m_GlobalOptions, bVerbose);
							
							for (auto pSection : Sections)
							{
								auto &Section = *pSection;
								auto SectionScope = fOutputHeading(Section.m_Heading);
								if (bVerbose)
									fOutputSectionedText(Section.m_Description);
								fOutputOptions("Shared Options", Section.m_SectionOptions, bVerbose);

								COnScopeExitShared IndentScope;
								IndentScope = fOutputHeading("Commands");
								
								for (auto &pCommand : SectionMap[pSection])
								{
									auto &Command = *pCommand;
									if (bVerbose)
										fOutputVerboseCommand(fGetCommandEntry(Command));
									else
										fOutputLine(fGetCommandUsage(fGetCommandEntry(Command)));
								}
								fOutputEndSection();
							}
							return 0;
						}
						
						COnScopeExitShared FirstSectionScope;
						
						for (auto &Section : Internal.m_Sections)
						{
							auto SectionScope = fOutputHeading(Section.m_Heading);
							if (bVerbose)
								fOutputSectionedText(Section.m_Description);
							
							if (bFirstSection)
							{
								{
									auto UsageScope = fOutputHeading("Usage");
									fOutputLine
										(
											fg_Format
											(	
												"{} [Command] [GlobalOptions] [SectionOptions] [CommandOptions] [--] [CommandParameters]"
												, NFile::CFile::fs_GetFileNoExt(NFile::CFile::fs_GetProgramPath())
											)
											, true
											, 2
										)
									;
								}
								
								fOutputOptions("Global Options", Internal.m_GlobalOptions, bVerbose);
							}
							
							TCMap<NStr::CStr, TCVector<CCommandEntry>> CommandEntries;
							mint LongestName = 0;

							if (bVerbose)
								fOutputOptions("Shared Options", Section.m_SectionOptions, bVerbose);
							
							for (auto &Command : Section.m_Commands)
							{
								auto &Entry = CommandEntries[Command.m_Category].f_Insert(fGetCommandEntry(Command));
								LongestName = fg_Max(aint(LongestName), Entry.m_Names.f_GetLen());
							}
							
							COnScopeExitShared IndentScope;
							if (bVerbose || bFirstSection)
							{
								IndentScope = fOutputHeading("Commands");
							}
							
							for (auto &Category : CommandEntries)
							{
								auto &CategoryName = CommandEntries.fs_GetKey(Category);
								COnScopeExitShared CategoryScope;
								if (!CategoryName.f_IsEmpty())
									CategoryScope = fOutputHeading(CategoryName); 
								for (auto &CommandEntry : Category)
								{
									bool bCommandVerbose = CommandEntry.m_pCommand->m_bAlwaysVerbose || bVerbose;
									if (bCommandVerbose)
										fOutputVerboseCommand(CommandEntry);
									else
										fOutputLine(fg_Format("{sl*,a-}   {}", CommandEntry.m_Names, LongestName, CommandEntry.m_pCommand->m_ShortDescription), false, LongestName + 5);
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
}
