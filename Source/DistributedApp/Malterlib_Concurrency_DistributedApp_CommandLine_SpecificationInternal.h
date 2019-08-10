// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NFile;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;

	struct CDistributedAppCommandLineSpecification::CInternal
	{
		enum EColor
		{
			EColor_None
			, EColor_Executable
			, EColor_Command
			, EColor_Number
			, EColor_String
			, EColor_Constant
			, EColor_Date
			, EColor_Binary
			, EColor_Parameter
			, EColor_GlobalOption
			, EColor_SectionOption
			, EColor_Option
			, EColor_Value
			, EColor_Error
			, EColor_Heading1
			, EColor_Default
		};

		struct CSection;
		struct CCommand;

		struct CValue
		{
			CValue(CStr const &_Identifier, bool _bOptional)
				: m_Identifier(_Identifier)
				, m_bOptional(_bOptional)
			{
			}
			void f_Parse(CEJSON const &_Option);
			CEJSON fp_ParseEJSON(CStr const &_Value, CStr const &_Error) const;
			CEJSON fp_ConvertValue(CEJSON const &_Template, CEJSON const &_Value, CStr const &_Identifier, bool _bStrict, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			CStr fp_FormatValue(CEJSON const &_Template, CEJSON const &_Value, CStr const &_Identifier, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			void fp_ValidateTemplate(CEJSON const &_Template, CStr const &_Identifier, bool _bPrevIsSetOf) const;
			CEJSON f_ConvertValue(CEJSON const &_Value, EColor _Color, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			CEJSON f_ConvertValue(CEJSON const &_Value, CStr const &_Identifier, EColor _Color, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			void f_AppendConvertValue(CEJSON &o_Value, CEJSON const &_Value, EColor _Color, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			void f_AppendConvertValue(CEJSON &o_Value, CEJSON const &_Value, CStr const &_Identifier, EColor _Color, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;
			CStr f_FormatValue(CEJSON const &_Value, NCommandLine::EAnsiEncodingFlag _AnsiFlags) const;

			CStr const m_Identifier;
			bool const m_bOptional;

			CEJSON m_TypeTemplate;
			CEJSON m_Default;

			CStr m_ShortDescription;
			CStr m_LongDescription;
		};

		struct COptionSet
		{
			TCSet<CStr> m_Allowed;
			TCSet<CStr> m_Disallowed;
			bool m_bAllowedSpecified = false;
		};

		struct COption : public CValue
		{
			COption(CStr const &_Identifier, TCVector<CStr> const &_Names, CCommand *_pCommand, bool _bOptional)
				: CValue(_Identifier, _bOptional)
				, m_pCommand(_pCommand)
				, m_Names(_Names)
			{
			}
			void f_ParseOption(CEJSON const &_Option);
			bool f_IsEnabled(COptionSet const &_OptionSet, bool _bIsDirect) const;

			TCVector<CStr> const m_Names;
			CCommand * const m_pCommand;
			bool m_bDefaultEnabled = true;
			bool m_bHidden = false;
			bool m_bCanNegate = true;
			bool m_bDisablesAllErrors = false;
			bool m_bValidForDirectCommand = true;
		};

		struct CParameter : public CValue
		{
			CParameter(CStr const &_Identifier, bool _bOptional, bool _bVector)
				: CValue(_Identifier, _bOptional)
				, m_bVector(_bVector)
			{
			}
			void f_ParseParameter(CEJSON const &_Parameter);

			bool m_bVector;
		};

		struct CCommand
		{
			CCommand(CSection *_pSection, TCVector<CStr> const &_Names)
				: m_pSection(_pSection)
				, m_Names(_Names)
			{
			}
			void f_RegisterOptions(CInternal &_Internal, NEncoding::CEJSON const &_Options);

			CSection * const m_pSection;
			TCVector<CStr> const m_Names;

			CStr m_Category;

			CStr m_ShortDescription;
			CStr m_LongDescription;
			CStr m_OutputDescription;
			TCMap<CStr, CStr> m_StatusDescription;

			TCLinkedList<CParameter> m_Parameters;
			TCMap<CStr, CParameter *> m_ParametersByIdentifier;

			TCLinkedList<COption> m_Options;
			TCMap<CStr, COption *> m_OptionsByIdentifier;
			TCMap<CStr, COption *> m_OptionsByName;

			COptionSet m_SectionOptionSet;
			COptionSet m_GlobalOptionSet;

			NStorage::TCSharedPointer
				<
					TCActorFunctor<TCFuture<uint32> (NEncoding::CEJSON const &_Parameters, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)>
				>
 				m_pActorRunCommand
			;
			NStorage::TCSharedPointer
				<
					NFunction::TCFunctionMovable<uint32 (NEncoding::CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)>
				>
				m_pDirectRunCommand
			;

			ECommandFlag m_Flags = ECommandFlag_None;

			bool m_bErrorOnCommandAsParameter = true;
			bool m_bErrorOnOptionAsParameter = true;
			bool m_bAlwaysVerbose = false;
			bool m_bGreedyDefaultCommand = false;
			bool m_bShowOptionsInCommandEntry = false;
			bool m_bShowParametersStart = true;
		};

		struct CSection
		{
			NStr::CStr m_Heading;
			NStr::CStr m_Description;
			TCLinkedList<CCommand> m_Commands;

			TCMap<CStr, TCVector<CStr>> m_CommandOptionsIdentifiers;
			TCMap<CStr, TCVector<CStr>> m_CommandParameterIdentifiers;
			TCMap<CStr, TCVector<CStr>> m_CommandOptionsNames;

			TCLinkedList<COption> m_SectionOptions;
			TCMap<CStr, COption *> m_SectionOptionsByIdentifier;
			TCMap<CStr, COption *> m_SectionOptionsByName;
		};

		CCommand *f_RegisterCommand(CSection &_Section, CEJSON const &_CommandDescription);
		CEJSON f_ValidateParams(CCommand const &_Command, CEJSON const &_Params) const;
		void f_CheckName(CStr const &_Name);
		static void fs_CheckValidObject(CEJSON const &_ToCheck, TCSet<CStr> const &_AllowedKeys);
		static CStr fs_Color(CStr const &_String, EColor _Color, NCommandLine::EAnsiEncodingFlag _AnsiFlags);

		TCLinkedList<CSection> m_Sections;
		TCLinkedList<COption> m_GlobalOptions;

		TCMap<CStr, CCommand *> m_CommandByName;

		TCMap<CStr, TCVector<CStr>> m_CommandOptionsIdentifiers;
		TCMap<CStr, TCVector<CStr>> m_CommandParameterIdentifiers;
		TCMap<CStr, TCVector<CStr>> m_CommandOptionsNames;

		TCMap<CStr, TCVector<CStr>> m_SectionOptionsIdentifiers;
		TCMap<CStr, TCVector<CStr>> m_SectionOptionsNames;

		TCMap<CStr, COption *> m_GlobalOptionsByIdentifier;
		TCMap<CStr, COption *> m_GlobalOptionsByName;

		CCommand *m_pDefaultCommand = nullptr;
	};
}
