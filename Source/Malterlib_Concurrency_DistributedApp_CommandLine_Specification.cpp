// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

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
		CDistributedAppCommandLineSpecification::CCommand::CCommand(CInternal *_pInternal, void *_pCommand)
			: mp_pInternal(_pInternal)
			, mp_pCommand(_pCommand)
		{
		}

		namespace 
		{
			CStr fg_ParseIdentifier(CStr const &_Name, bool &o_bIsOptional, bool &o_bIsVector)
			{
				CStr Identifier = _Name;
				o_bIsOptional = false;
				if (Identifier[Identifier.f_GetLen() - 1] == '?')
				{
					o_bIsOptional = true;
					Identifier = Identifier.f_Left(Identifier.f_GetLen() - 1); 
				}

				o_bIsVector = false;
				if (Identifier.f_FindReverse("...") == Identifier.f_GetLen() - 3)
				{
					o_bIsVector = true;
					Identifier = Identifier.f_Left(Identifier.f_GetLen() - 3);
				}
				
				return Identifier;
			}
			
			void fg_ParseDescription(CEJSON const &_Object, CStr &o_Short, CStr &o_Long)
			{
				CStr Description = _Object["Description"].f_String();
				o_Short = fg_GetStrLineSep(Description).f_Trim();
				o_Long = Description.f_Trim();
			}
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::CValue::f_Parse(CEJSON const &_Value)
		{
			if (auto *pDefault = _Value.f_GetMember("Default"))
				m_Default = *pDefault; 
			if (auto *pType = _Value.f_GetMember("Type"))
				m_TypeTemplate = *pType;
			
			if (!m_Default.f_IsValid() && !m_TypeTemplate.f_IsValid())
				DMibError("An option needs to specify at least 'Default' or 'Type'");
			else if (m_Default.f_IsValid() && !m_TypeTemplate.f_IsValid())
				m_TypeTemplate = m_Default;

			fp_ValidateTemplate(m_TypeTemplate, m_Identifier, false);
			
			if (m_Default.f_IsValid())
				m_Default = f_ConvertValue(m_Default); // This will do checking to make sure that the default value matches the template
			
			fg_ParseDescription(_Value, m_ShortDescription, m_LongDescription);
		}

		CEJSON CDistributedAppCommandLineSpecification::CInternal::f_ValidateParams(CCommand const &_Command, CEJSON const &_Params) const
		{
			CEJSON Params;

			auto fCheckValue = [&](CValue const &_Value)
				{
					if (!_Params.f_GetMember(_Value.m_Identifier))
					{
						if (!_Value.m_bOptional || _Value.m_Default.f_IsValid())
							DMibError(fg_Format("Missing non-optional parameter: {}", _Value.m_Identifier));
					}
				}
			;
			
			for (auto &Option : m_GlobalOptions)
				fCheckValue(Option);
			for (auto &Option : _Command.m_pSection->m_SectionOptions)
				fCheckValue(Option);
			for (auto &Option : _Command.m_Options)
				fCheckValue(Option);
			for (auto &Parameter : _Command.m_Parameters)
				fCheckValue(Parameter);
			
			for (auto &Param : _Params.f_Object())
			{
				CValue *pValue = nullptr;
				CStr const &ParamName = Param.f_Name();
				if (auto pParameter = _Command.m_ParametersByIdentifier.f_FindEqual(ParamName))
					pValue = *pParameter;
				else if (auto pOption = _Command.m_OptionsByIdentifier.f_FindEqual(ParamName))
					pValue = *pOption;
				else if (auto pOption = _Command.m_pSection->m_SectionOptionsByIdentifier.f_FindEqual(ParamName))
					pValue = *pOption;
				else if (auto pOption = m_GlobalOptionsByIdentifier.f_FindEqual(ParamName))
					pValue = *pOption;
				if (!pValue)
					DMibError(fg_Format("Unexpected parameter '{}' in command params", ParamName));
				Params[ParamName] = pValue->f_ConvertValue(Param.f_Value());
			}

			return Params;
		}
		
		CEJSON CDistributedAppCommandLineSpecification::CInternal::CValue::f_ConvertValue(CEJSON const &_Value) const
		{
			return fp_ConvertValue(m_TypeTemplate, _Value, m_Identifier);
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::CValue::f_AppendConvertValue(CEJSON &o_Value, CEJSON const &_Value) const
		{
			CEJSON NewParams = fp_ConvertValue(m_TypeTemplate.f_Array()[0], _Value, m_Identifier);
			o_Value.f_Array().f_Insert(NewParams);
		}
		
		CEJSON CDistributedAppCommandLineSpecification::CInternal::CValue::fp_ParseEJSON(CStr const &_Value, CStr const &_Error, CStr const &_Identifier) const
		{
			try
			{
				return CEJSON::fs_FromString(_Value);
			}
			catch (NException::CException const &_Exception)
			{
				DMibError(fg_Format("[{}] {}: {}", _Identifier, _Error, _Exception.f_GetErrorStr())); 
			}
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::CValue::fp_ValidateTemplate(CEJSON const &_Template, CStr const &_Identifier, bool _bPrevIsSetOf) const
		{
			switch (_Template.f_Type())
			{
			case EJSONType_String:
			case EJSONType_Integer:
			case EJSONType_Float:
			case EJSONType_Boolean:
			case EEJSONType_Binary:
			case EEJSONType_Date:
				break;
			case EEJSONType_UserType:
				{
					auto &TemplateUserType = _Template.f_UserType();
					if (TemplateUserType.m_Type == "$OneOf" || TemplateUserType.m_Type == "$OneOfType")
					{
						bool bIsType = TemplateUserType.m_Type == "$OneOfType";
						CEJSON TemplateSet = CEJSON::fs_FromJSON(TemplateUserType.m_Value);
						
						auto &Set = TemplateSet.f_Array();
						
						CStr::CFormat FormatIdentifier("{}.{{{}}");
						if (bIsType)
							FormatIdentifier.f_SetFormatStr("{}.({})");

						mint iSet = 0;
						FormatIdentifier << iSet; 
						
						for (auto &PossibleMatch : Set)
						{
							fp_ValidateTemplate(PossibleMatch, FormatIdentifier, !bIsType);
							++iSet;
						}
					}
				}
				break;
			case EJSONType_Object:
				{
					auto &TemplateObject = _Template.f_Object();
					
					TCSet<CStr> TemplateMemberNames;
					
					for (auto &TemplateMember : TemplateObject)
					{
						bool bOptional = false;
						bool bVector = false;
						CStr Identifier = fg_ParseIdentifier(TemplateMember.f_Name(), bOptional, bVector);
						
						if (bVector)
							DMibError("... vector form is not supported supported for object members");
						
						if (!TemplateMemberNames(Identifier).f_WasCreated())
							DMibError(fg_Format("[{}] Duplicate member '{}' in template object", _Identifier, Identifier));
						
						fp_ValidateTemplate(TemplateMember.f_Value(), fg_Format("{}.{}", _Identifier, Identifier), false);
					}
				}
				break;
			case EJSONType_Array:
				{
					auto &TemplateArray = _Template.f_Array();
					if (TemplateArray.f_IsEmpty())
						break;

					if (TemplateArray.f_GetLen() > 1)
						DMibError(fg_Format("[{}] Template array can only contain one value. Use COneOf or COneOfType to specify different options.", _Identifier));

					auto &Template = TemplateArray.f_GetFirst();
					fp_ValidateTemplate(Template, fg_Format("{}.[]", _Identifier), false); 
				}
				break;
			case EJSONType_Null:
				if (_bPrevIsSetOf)
					break;
				DMibError("null can only be used in COneOf statements");
				break;
			default:
				DMibError("Invalid template type");
				break;
			}
		}

		CEJSON CDistributedAppCommandLineSpecification::CInternal::CValue::fp_ConvertValue(CEJSON const &_Template, CEJSON const &_Value, CStr const &_Identifier) const
		{
			CEJSON Return;
			switch (_Template.f_Type())
			{
			case EJSONType_String:
				{
					Return = _Value.f_AsString();
				}
				break;
			case EJSONType_Integer:
				{
					if (_Value.f_IsString())
					{
						int64 Value = _Value.f_String().f_ToInt(TCLimitsInt<int64>::mc_Max);
						if (Value == TCLimitsInt<int64>::mc_Max)
						{
							Value = _Value.f_String().f_ToInt(TCLimitsInt<int64>::mc_Min);
							if (Value == TCLimitsInt<int64>::mc_Min)
								DMibError(fg_Format("[{}] Failed to parse '{}' as a integer value", _Identifier, _Value.f_String()));
						}
						Return = Value;
					}
					else
						Return = _Value.f_AsInteger();
				}
				break;
			case EJSONType_Float:
				{
					if (_Value.f_IsString())
					{
						fp64 Value = _Value.f_String().f_ToFloat(fp64::fs_Inf());
						if (Value == fp64::fs_Inf())
							DMibError(fg_Format("[{}] Failed to parse '{}' as a float value", _Identifier, _Value.f_String()));
						Return = Value;
					}
					else
						Return = _Value.f_AsFloat();
				}
				break;
			case EJSONType_Boolean:
				{
					if (_Value.f_IsString())
					{
						if (_Value.f_String() == "true")
							Return = true;
						else if (_Value.f_String() == "false")
							Return = false;
						else
						{
							int64 Value = _Value.f_String().f_ToInt(TCLimitsInt<int64>::mc_Max);
							if (Value == 1)
								Return = true;
							else if (Value == 0)
								Return = false;
							else
								DMibError(fg_Format("[{}] Failed to parse '{}' as a boolean value", _Identifier, _Value.f_String()));
						}
					}
					else
						Return = _Value.f_AsBoolean();
				}
				break;
			case EEJSONType_Binary:
				{
					if (_Value.f_IsBinary())
						Return = _Value;
					else if (_Value.f_IsString())
					{
						NContainer::TCVector<uint8> Data;
						NDataProcessing::fg_Base64Decode(_Value.f_String(), Data);
						Return = Data;
					}
					else
						DMibError(fg_Format("[{}] Binary can only be converted from string", _Identifier));
				}
				break;
			case EEJSONType_Date:
				{
					if (_Value.f_IsDate())
					{
						Return = _Value;
						break;
					}
					if (!_Value.f_IsString())
						DMibError(fg_Format("[{}] Date can only be converted from string", _Identifier));
					
					auto fReportError = [&](CStr const &_Error)
						{
							DMibError(fg_Format("[{}] Failed to parse date: {}. Date format is: Year-Month-Day [Hour[:Minute[:Second[.Fraction]]]]", _Identifier, _Error));
						}
					;
					
					auto fParseInt = [&](ch8 const *&_pParse, auto _Type, ch8 const *_pTerminators)
						{
							bool bFailed = false;
							auto Return = fg_StrToIntParse(_pParse, _Type, _pTerminators, false, EStrToIntParseMode_Base10, &bFailed);
							if (bFailed)
								fReportError(fg_Format("Failed to parse '{}' as a integer", _pParse));
							if (*_pParse == _pTerminators[0])
								++_pParse;
							return Return;
						}
					;

					ch8 const *pParse = _Value.f_String();
					
					fg_ParseWhiteSpace(pParse);
					if (!*pParse)
						fReportError("Missing year");
					int64 Year = fParseInt(pParse, int64(), "-");
					++pParse;
					if (!*pParse)
						fReportError("Missing month");
					uint32 Month = fParseInt(pParse, uint32(), "-");
					if (!*pParse)
						fReportError("Missing day");
					uint32 Day = fParseInt(pParse, uint32(), " ");
					uint32 Hour = 0;
					uint32 Minute = 0;
					uint32 Second = 0;
					fp64 Fraction = 0;
					if (*pParse)
						Hour = fParseInt(pParse, uint32(), ":");
					if (*pParse)
						Minute = fParseInt(pParse, uint32(), ":");
					if (*pParse)
						Second = fParseInt(pParse, uint32(), ".");
					if (*pParse)
					{
						--pParse;
						Fraction = fg_StrToFloatParse(pParse, fp64::fs_Inf(), (ch8 const *)nullptr, false, (ch8 const *)nullptr);
						if (Fraction == fp64::fs_Inf())
							fReportError(fg_Format("Failed to parse '{}' as a float", pParse));
					}
					
					if (*pParse)
						fReportError(fg_Format("Unexpected: {}", pParse));

					if (fg_Clamp(Month, 1, 12) != Month)
						fReportError("Invalid month");
					if (fg_Clamp(Day, 1, NTime::CTimeConvert::fs_GetDaysInMonth(Year, Month - 1)) != Day)
						fReportError("Invalid day");
					if (fg_Clamp(Hour, 0, 23) != Hour)
						fReportError("Invalid hour");
					if (fg_Clamp(Minute, 0, 59) != Minute)
						fReportError("Invalid minute");
					if (fg_Clamp(Second, 0, 59) != Second)
						fReportError("Invalid second");
					if (fg_Clamp(Fraction, 0.0, 1.0) != Fraction)
						fReportError("Invalid fraction");
					
					Return = NTime::CTimeConvert::fs_CreateTime(Year, Month, Day, Hour, Minute, Second, Fraction);
				}
				break;
			case EEJSONType_UserType:
				{
					auto &TemplateUserType = _Template.f_UserType();
					if (TemplateUserType.m_Type == "$OneOf" || TemplateUserType.m_Type == "$OneOfType")
					{
						bool bIsType = TemplateUserType.m_Type == "$OneOfType";
						CEJSON TemplateSet = CEJSON::fs_FromJSON(TemplateUserType.m_Value);
						
						auto &Set = TemplateSet.f_Array();  
						
						CStr::CFormat FormatIdentifier("{}.{{{}}");
						if (bIsType)
							FormatIdentifier.f_SetFormatStr("{}.({})");

						mint iSet = 0;
						FormatIdentifier << iSet;
						
						for (auto &PossibleMatch : Set)
						{
							try
							{
								CEJSON Value = fp_ConvertValue(PossibleMatch, _Value, FormatIdentifier);
								if (!bIsType || Value == PossibleMatch)
									return Value;
							}
							catch (NException::CException const &_Exception)
							{
							}
							++iSet;
						}
						
						DMibError(fg_Format("[{}] Could not match '{jp}' to any member in set", _Identifier, _Value));
						
						break;
					}
					if (_Value.f_IsUserType())
					{
						auto &UserType = _Value.f_UserType();
						if (UserType.m_Type != TemplateUserType.m_Type)
							DMibError(fg_Format("[{}] Incorrect user type: '{}' != '{}'", _Identifier, UserType.m_Type, TemplateUserType.m_Type));
						Return = _Value;
						break;
					}
					DMibError("Converting to user type is not supported");
				}
				break;
			case EJSONType_Object:
				{
					CEJSON RawValue;
					if (_Value.f_IsObject())
						RawValue = _Value;
					else if (_Value.f_IsString())
					{
						RawValue = fp_ParseEJSON(_Value.f_String(), "Error parsing object parameter", _Identifier);
						if (!RawValue.f_IsObject())
							DMibError(fg_Format("[{}] Expected object", _Identifier));
					}
					else
						DMibError(fg_Format("[{}] Object can only be converted from string", _Identifier));
					
					auto &TemplateObject = _Template.f_Object();
					auto &InputObject = RawValue.f_Object();
					auto &OutputObject = Return.f_Object();
					
					TCSet<CStr> TemplateMemberNames;
					
					for (auto &TemplateMember : TemplateObject)
					{
						bool bOptional = false;
						bool bVector = false;
						CStr Identifier = fg_ParseIdentifier(TemplateMember.f_Name(), bOptional, bVector);
						
						DMibCheck(!bVector);

						TemplateMemberNames[Identifier];
						
						auto *pInputMember = InputObject.f_GetMember(Identifier);
						if (!pInputMember)
						{
							if (!bOptional)
								DMibError(fg_Format("[{}] Missing reqired member '{}' in object", _Identifier, TemplateMember.f_Name()));
							continue;
						}
						
						OutputObject.f_CreateMember(Identifier) = fp_ConvertValue(TemplateMember.f_Value(), *pInputMember, fg_Format("{}.{}", _Identifier, Identifier));
					}
					
					for (auto &InputMember : InputObject)
					{
						if (!TemplateMemberNames.f_FindEqual(InputMember.f_Name()))
							DMibError(fg_Format("Unexpected member '{}' in input object", InputMember.f_Name()));
					}
				}
				break;
			case EJSONType_Array:
				{
					CEJSON RawValue;
					if (_Value.f_IsArray())
						RawValue = _Value;
					else if (_Value.f_IsString())
					{
						RawValue = fp_ParseEJSON(_Value.f_String(), "Error parsing array parameter", _Identifier);
						if (!RawValue.f_IsArray())
							DMibError("Expected array");
					}
					else
						DMibError("Array can only be converted from string");

					auto &TemplateArray = _Template.f_Array();
					if (TemplateArray.f_IsEmpty())
					{
						Return = RawValue;
						break;
					}

					DMibCheck(TemplateArray.f_GetLen() == 1);

					auto &OutputArray = Return.f_Array();
					auto &InputArray = RawValue.f_Array();
					if (TemplateArray.f_GetLen() == 1)
					{
						auto &Template = TemplateArray.f_GetFirst();
						CStr Identifier = fg_Format("{}.[]", _Identifier);
						for (auto &InputElement : InputArray)
							OutputArray.f_Insert() = fp_ConvertValue(Template, InputElement, Identifier);
					}

					Return = RawValue;
				}
				break;
			case EJSONType_Null:
				if (_Value.f_IsNull())
				{
					Return = _Value;
					break;
				}
			default:
				DMibError("Invalid template type");
				break;
			}
			
			return Return;			
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::COption::f_ParseOption(CEJSON const &_Option)
		{
			fs_CheckValidObject(_Option, {"Type", "Default", "Names", "Description", "DefaultEnabled"});
			
			CValue::f_Parse(_Option);
			
			if (auto *pDefaultEnabled = _Option.f_GetMember("DefaultEnabled"))
				m_bDefaultEnabled = pDefaultEnabled->f_Boolean();
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::CParameter::f_ParseParameter(CEJSON const &_Parameter)
		{
			fs_CheckValidObject(_Parameter, {"Type", "Default", "Description"});
			CValue::f_Parse(_Parameter);
			
			if (m_bVector)
			{
				if (!m_TypeTemplate.f_IsArray())
					DMibError("A ... parameter needs to have an array type");
				
				if (m_TypeTemplate.f_Array().f_GetLen() != 1)
					DMibError("A ... parameter needs to have a specified type in the array");
			}
		}

		void CDistributedAppCommandLineSpecification::CInternal::fs_CheckValidObject(CEJSON const &_ToCheck, TCSet<CStr> const &_AllowedKeys)
		{
			for (auto &Member : _ToCheck.f_Object())
			{
				if (!_AllowedKeys.f_FindEqual(Member.f_Name()))
					DMibError(fg_Format("Unexpected member '{}'",Member.f_Name()));
			}
		}
		
		void CDistributedAppCommandLineSpecification::CInternal::f_CheckName(CStr const &_Name)
		{
			if (_Name.f_StartsWith("--no-"))
			{
				DMibError
					(
						"An option or command name cannot start with --no- as this is reserved for negating a boolean option."
						" If this is what you are trying to achieve, rather use then non-negative form and set the default to true."
					)
				;
			}
			if (_Name.f_FindChar('=') > 0)
				DMibError("An option or command name cannot contain '=' as this can be used to specify the value for an option.");
		}

		void CDistributedAppCommandLineSpecification::CInternal::CCommand::f_RegisterOptions(CInternal &_Internal, NEncoding::CEJSON const &_Options)
		{
			auto &Internal = _Internal;
			
			for (auto &Option : _Options.f_Object())
			{
				bool bOptional = false;
				bool bVector = false;
				CStr Identifier = fg_ParseIdentifier(Option.f_Name(), bOptional, bVector);
				
				if (bVector)
					DMibError("... vector form is not supported supported for options");
 
				if (Internal.m_GlobalOptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("Option with same identifier '{}' already exists in global options", Identifier));
				
				if (m_pSection->m_SectionOptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("Option with same identifier '{}' already exists in section options", Identifier));
				
				if (m_OptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("Option with identifier '{}' already exists on command", Identifier));
				
				if (m_ParametersByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("Parameter with identifier '{}' already exists on command", Identifier));

				auto &NameArray = Option.f_Value()["Names"].f_Array();
				if (NameArray.f_IsEmpty())
					DMibError("You need to specify at least one name for option");
				
				TCVector<CStr> Names;
				for (auto &NameJSON : NameArray)
				{
					auto &Name = NameJSON.f_String();
					Internal.f_CheckName(Name);
					if (Internal.m_CommandByName.f_FindEqual(Name))
						DMibError(fg_Format("Name is already used as a command '{}'", Name));
					if (m_pSection->m_SectionOptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("Option with same name '{}' already exists in section options", Name));
					if (Internal.m_GlobalOptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("Option with same name '{}' already exists in global options", Name));
					if (m_OptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("Option with name '{}' already exists on command", Name));
					Names.f_Insert(Name);
				}
				
				auto &NewOption = m_Options.f_Insert(fg_Construct(Identifier, Names, this, bOptional));
				m_OptionsByIdentifier[Identifier] = &NewOption;
				Internal.m_CommandOptionsIdentifiers[Identifier].f_Insert(m_Names.f_GetFirst()); 
				m_pSection->m_CommandOptionsIdentifiers[Identifier].f_Insert(m_Names.f_GetFirst()); 
				
				for (auto &NameJSON : NameArray)
				{
					auto &Name = NameJSON.f_String();
					Internal.m_CommandOptionsNames[Name].f_Insert(m_Names.f_GetFirst());
					m_OptionsByName[Name] = &NewOption;
				}
				
				NewOption.f_ParseOption(Option.f_Value());
				
				if (!NewOption.m_bDefaultEnabled)
					DMibError("Default enabled can only be disabled on section options");
			}
		}
		
		void CDistributedAppCommandLineSpecification::CCommand::f_RegisterOptions(NEncoding::CEJSON const &_Options)
		{
			auto &Internal = *mp_pInternal;
			CInternal::CCommand *pCommand = fg_AutoStaticCast(mp_pCommand);
			pCommand->f_RegisterOptions(Internal, _Options);
		}
		
		CDistributedAppCommandLineSpecification::CSection::CSection(CInternal *_pInternal, void *_pSection)
			: mp_pInternal(_pInternal)
			, mp_pSection(_pSection)
		{
		}

		template <typename tf_CIdentifiers>
		CStr fg_GetFormattedIdentifiers(tf_CIdentifiers &_Identifiers)
		{
			CStr Commands;
			for (auto &CommandIdentifier : _Identifiers)
				fg_AddStrSep(Commands, CommandIdentifier, ",");
			return Commands;
		}

		void CDistributedAppCommandLineSpecification::CSection::f_RegisterSectionOptions(NEncoding::CEJSON const &_Options)
		{
			CInternal::CSection *pSection = fg_AutoStaticCast(mp_pSection);
			auto &Section = *pSection;
			auto &Internal = *mp_pInternal;

			for (auto &Option : _Options.f_Object())
			{
				bool bOptional = false;
				bool bVector = false;
				CStr Identifier = fg_ParseIdentifier(Option.f_Name(), bOptional, bVector);
				
				if (bVector)
					DMibError("... vector form is not supported supported for options");
				if (Internal.m_GlobalOptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("A global option with identifier '{}' already exists", Identifier));
				if (Section.m_SectionOptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("A section option with identifier '{}' already exists.", Identifier));
				if (auto pIdentifiers = Section.m_CommandOptionsIdentifiers.f_FindEqual(Identifier))
					DMibError(fg_Format("Option with same identifier '{}' already exists in command(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				if (auto pIdentifiers = Section.m_CommandParameterIdentifiers.f_FindEqual(Identifier))
					DMibError(fg_Format("Command parameter with same identifier '{}' already exists in command(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				
				auto &NameArray = Option.f_Value()["Names"].f_Array();
				if (NameArray.f_IsEmpty())
					DMibError("You need to specify at least one name for option");

				TCVector<CStr> Names;
				for (auto &NameJSON : NameArray)
				{
					auto &Name = NameJSON.f_String();
					Internal.f_CheckName(Name);
					if (Internal.m_CommandByName.f_FindEqual(Name))
						DMibError(fg_Format("Name is already used as a command '{}'", Name));
					if (Internal.m_GlobalOptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("A global option with same name '{}' already exists", Name));
					if (Section.m_SectionOptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("A section option with name '{}' already exists", Name));
					if (auto pIdentifiers = Section.m_CommandOptionsNames.f_FindEqual(Name))
						DMibError(fg_Format("Option with name '{}' already exists in command(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
					Names.f_Insert(Name);
				}
				
				auto &NewOption = Section.m_SectionOptions.f_Insert(fg_Construct(Identifier, Names, nullptr, bOptional));
				Section.m_SectionOptionsByIdentifier[Identifier] = &NewOption;
				Internal.m_SectionOptionsIdentifiers[Identifier].f_Insert(Section.m_Heading); 
				
				for (auto &NameJSON : NameArray)
				{
					auto &Name = NameJSON.f_String();
					Section.m_SectionOptionsByName[Name] = &NewOption;
					Internal.m_SectionOptionsNames[Name].f_Insert(Section.m_Heading);
				}
				
				NewOption.f_ParseOption(Option.f_Value());
			}
		}
		
		void CDistributedAppCommandLineSpecification::f_RegisterGlobalOptions(NEncoding::CEJSON const &_Options)
		{
			auto &Internal = *mp_pInternal;

			for (auto &Option : _Options.f_Object())
			{
				bool bOptional = false;
				bool bVector = false;
				CStr Identifier = fg_ParseIdentifier(Option.f_Name(), bOptional, bVector);
				
				if (bVector)
					DMibError("... vector form is not supported supported for options");
				if (Internal.m_GlobalOptionsByIdentifier.f_FindEqual(Identifier))
					DMibError(fg_Format("A global option with identifier '{}' already exists", Identifier));
				if (auto pIdentifiers = Internal.m_SectionOptionsIdentifiers.f_FindEqual(Identifier))
					DMibError(fg_Format("Section option with same identifier '{}' already exists in sections(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				if (auto pIdentifiers = Internal.m_CommandOptionsIdentifiers.f_FindEqual(Identifier))
					DMibError(fg_Format("Option with same identifier '{}' already exists in command(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				if (auto pIdentifiers = Internal.m_CommandParameterIdentifiers.f_FindEqual(Identifier))
					DMibError(fg_Format("Command parameter with same identifier '{}' already exists in command(s): {}", Identifier, fg_GetFormattedIdentifiers(*pIdentifiers)));
				
				auto &NameArray = Option.f_Value()["Names"].f_Array();
				if (NameArray.f_IsEmpty())
					DMibError("You need to specify at least one name for option");

				TCVector<CStr> Names;
				for (auto &NameJSON : NameArray)
				{
					auto &Name = NameJSON.f_String();
					Internal.f_CheckName(Name);
					if (Internal.m_CommandByName.f_FindEqual(Name))
						DMibError(fg_Format("Name is already used as a command '{}'", Name));
					if (Internal.m_GlobalOptionsByName.f_FindEqual(Name))
						DMibError(fg_Format("A global option with same name '{}' already exists", Name));
					if (auto pIdentifiers = Internal.m_SectionOptionsNames.f_FindEqual(Name))
						DMibError(fg_Format("Section option with name '{}' already exists in sections(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
					if (auto pIdentifiers = Internal.m_CommandOptionsNames.f_FindEqual(Name))
						DMibError(fg_Format("Option with name '{}' already exists in command(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
					Names.f_Insert(Name);
				}
				
				auto &NewOption = Internal.m_GlobalOptions.f_Insert(fg_Construct(Identifier, Names, nullptr, bOptional));
				Internal.m_GlobalOptionsByIdentifier[Identifier] = &NewOption;
				
				for (auto &NameJSON : NameArray)
					Internal.m_GlobalOptionsByName[NameJSON.f_String()] = &NewOption;
				
				NewOption.f_ParseOption(Option.f_Value());

				if (!NewOption.m_bDefaultEnabled)
					DMibError("Default enabled can only be disabled on section options");
			}
		}

		auto CDistributedAppCommandLineSpecification::CInternal::f_RegisterCommand(CSection &_Section, CEJSON const &_CommandDescription) -> CCommand *
		{
			fs_CheckValidObject
				(
					_CommandDescription
					, 
					{
						"Names"
						, "Description"
						, "Status"
						, "Output"
						, "ErrorOnCommandAsParameter"
						, "ErrorOnOptionAsParameter"
						, "AlwaysVerbose"
						, "Parameters"
						, "Options"
						, "SectionOptions"
						, "DisableSectionOptions"
						, "Category"
					}
				)
			;

			auto &NameArray = _CommandDescription["Names"].f_Array();
			if (NameArray.f_IsEmpty())
				DMibError("You need to specify at least one name for command");
			
			TCVector<CStr> Names;
			for (auto &NameJSON : NameArray)
			{
				auto &Name = NameJSON.f_String();
				f_CheckName(Name);
				if (m_GlobalOptionsByName.f_FindEqual(Name))
					DMibError(fg_Format("A global option with same name '{}' already exists", Name));
				if (auto pIdentifiers = m_SectionOptionsNames.f_FindEqual(Name))
					DMibError(fg_Format("Section option with name '{}' already exists in section(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
				if (auto pIdentifiers = m_CommandOptionsNames.f_FindEqual(Name))
					DMibError(fg_Format("Option with name '{}' already exists in command(s): {}", Name, fg_GetFormattedIdentifiers(*pIdentifiers)));
				if (m_CommandByName.f_FindEqual(Name))
					DMibError(fg_Format("Name is already used for another command '{}'", Name));
				Names.f_Insert(Name);
			}

			auto &NewCommand = _Section.m_Commands.f_Insert(fg_Construct(&_Section, Names));

			for (auto &NameJSON : NameArray)
			{
				auto &Name = NameJSON.f_String();
				m_CommandByName[Name] = &NewCommand; 
			}
			
			fg_ParseDescription(_CommandDescription, NewCommand.m_ShortDescription, NewCommand.m_LongDescription);
			
			if (auto *pStatus = _CommandDescription.f_GetMember("Status"))
			{
				for (auto &Status : pStatus->f_Object())
					NewCommand.m_StatusDescription[Status.f_Name()] = Status.f_Value().f_String();
			}
			if (auto *pOutput = _CommandDescription.f_GetMember("Output"))
				NewCommand.m_OutputDescription = pOutput->f_String();
			if (auto *pCategory = _CommandDescription.f_GetMember("Category"))
				NewCommand.m_Category = pCategory->f_String();
			if (auto *pErrorOnCommandAsParameter = _CommandDescription.f_GetMember("ErrorOnCommandAsParameter"))
				NewCommand.m_bErrorOnCommandAsParameter = pErrorOnCommandAsParameter->f_Boolean();
			if (auto *pErrorOnOptionAsParameter = _CommandDescription.f_GetMember("ErrorOnOptionAsParameter"))
				NewCommand.m_bErrorOnOptionAsParameter = pErrorOnOptionAsParameter->f_Boolean();
			if (auto *pAlwaysVerbose = _CommandDescription.f_GetMember("AlwaysVerbose"))
				NewCommand.m_bAlwaysVerbose = pAlwaysVerbose->f_Boolean();
			if (auto *pSectionOptions = _CommandDescription.f_GetMember("SectionOptions"))
			{
				for (auto &Option : pSectionOptions->f_Array())
					NewCommand.m_AllowedSectionOptions[Option.f_String()];
			}
			if (auto *pSectionOptions = _CommandDescription.f_GetMember("DisableSectionOptions"))
			{
				for (auto &Option : pSectionOptions->f_Array())
					NewCommand.m_DisallowedSectionOptions[Option.f_String()];
			}

			if (auto *pParameters = _CommandDescription.f_GetMember("Parameters"))
			{
				bool bWasOptional = false;
				bool bWasVector = false;
				for (auto &Parameter : pParameters->f_Object())
				{
					bool bOptional = false;
					bool bVector = false;
					CStr Identifier = fg_ParseIdentifier(Parameter.f_Name(), bOptional, bVector);
					
					if (bWasVector)
						DMibError("Previous parameter was a vector so no further parameters can be specified.");
					if (bWasOptional && !bOptional)
						DMibError("Previous parameter was optional, but this one is not. This does not make sense.");
					
					bWasVector = bVector;
					bWasOptional = bOptional;
					if (NewCommand.m_ParametersByIdentifier.f_FindEqual(Identifier))
						DMibError(fg_Format("Duplicate parameter identifier '{}'", Identifier));
					if (m_GlobalOptionsByIdentifier.f_FindEqual(Identifier))
						DMibError(fg_Format("Option with same identifier '{}' already exists in global options", Identifier));
					if (_Section.m_SectionOptionsByIdentifier.f_FindEqual(Identifier))
						DMibError(fg_Format("Option with same identifier '{}' already exists in section options", Identifier));
						
					auto &NewParameter = NewCommand.m_Parameters.f_Insert(fg_Construct(Identifier, bOptional, bVector));
					NewCommand.m_ParametersByIdentifier[Identifier] = &NewParameter;
					m_CommandParameterIdentifiers[Identifier].f_Insert(NewCommand.m_Names.f_GetFirst()); 
					_Section.m_CommandParameterIdentifiers[Identifier].f_Insert(NewCommand.m_Names.f_GetFirst()); 
						
					NewParameter.f_ParseParameter(Parameter.f_Value());
				}
				
			}
			
			if (auto *pOptions = _CommandDescription.f_GetMember("Options"))
				NewCommand.f_RegisterOptions(*this, *pOptions);
			
			return &NewCommand;
		}
		
		auto CDistributedAppCommandLineSpecification::CSection::f_RegisterCommand
			(
				NEncoding::CEJSON const &_CommandDescription
				, NFunction::TCFunction<TCContinuation<CDistributedAppCommandLineResults> (NEncoding::CEJSON const &_Parameters)> const &_fRunCommand
			)
			-> CCommand
		{
			CInternal::CSection *pSection = fg_AutoStaticCast(mp_pSection);
			auto &Section = *pSection;
			auto &Internal = *mp_pInternal;
			auto *pCommand = Internal.f_RegisterCommand(Section, _CommandDescription);
			pCommand->m_fActorRunCommand = _fRunCommand;
			return CCommand(mp_pInternal, pCommand);
		}
		
		auto CDistributedAppCommandLineSpecification::CSection::f_RegisterDirectCommand
			(
				NEncoding::CEJSON const &_CommandDescription
				, NFunction::TCFunction<uint32 (NEncoding::CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)> const &_fRunCommand
			)
			-> CCommand 
		{
			CInternal::CSection *pSection = fg_AutoStaticCast(mp_pSection);
			auto &Section = *pSection;
			auto &Internal = *mp_pInternal;
			auto *pCommand = Internal.f_RegisterCommand(Section, _CommandDescription);
			pCommand->m_fDirectRunCommand = _fRunCommand;
			return CCommand(mp_pInternal, pCommand);
		}

		CDistributedAppCommandLineSpecification::CDistributedAppCommandLineSpecification()
			: mp_pInternal(fg_Construct()) 
		{
			f_AddSection("", "");
			fp_SetupDefaultCommands();
		}
		
		CDistributedAppCommandLineSpecification::~CDistributedAppCommandLineSpecification() noexcept
		{
		}

		CDistributedAppCommandLineSpecification::CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification const &_Other)
			: mp_pInternal(fg_Construct(*_Other.mp_pInternal))
		{
		}
		
		CDistributedAppCommandLineSpecification &CDistributedAppCommandLineSpecification::operator =(CDistributedAppCommandLineSpecification const &_Other)
		{
			mp_pInternal = fg_Construct(*_Other.mp_pInternal);
			return *this;
		}
		
		CDistributedAppCommandLineSpecification::CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification &&_Other) = default;
		CDistributedAppCommandLineSpecification &CDistributedAppCommandLineSpecification::operator =(CDistributedAppCommandLineSpecification &&_Other) = default;
		
		auto CDistributedAppCommandLineSpecification::f_GetDefaultSection() -> CSection
		{
			auto &Internal = *mp_pInternal;
			return CSection(&Internal, &Internal.m_Sections.f_GetFirst());
		}

		void CDistributedAppCommandLineSpecification::f_SetProgramDescription(NStr::CStr const &_Heading, NStr::CStr const &_Description)
		{
			auto &Internal = *mp_pInternal;
			auto &DefaultSection = Internal.m_Sections.f_GetFirst();
			DefaultSection.m_Heading = _Heading;
			DefaultSection.m_Description = _Description;
		}

		auto CDistributedAppCommandLineSpecification::f_AddSection(NStr::CStr const &_Heading, NStr::CStr const &_Description) -> CSection
		{
			auto &Internal = *mp_pInternal;
			auto &NewSection = Internal.m_Sections.f_Insert();
			NewSection.m_Heading = _Heading;
			NewSection.m_Description = _Description;
			return CSection(&Internal, &NewSection);
		}
		
		void CDistributedAppCommandLineSpecification::f_SetDefaultCommand(CCommand const &_Command)
		{
			auto &Internal = *mp_pInternal;
			Internal.m_pDefaultCommand = fg_AutoStaticCast(_Command.mp_pCommand);
		}
		
		void CDistributedAppCommandLineSpecification::fp_SetupDefaultCommands()
		{
			fp_SetupHelpCommand();
		}
	}		
}
