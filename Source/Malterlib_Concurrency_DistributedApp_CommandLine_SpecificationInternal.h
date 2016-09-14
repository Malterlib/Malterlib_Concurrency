// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib
{
	using namespace NStr;
	using namespace NFile;
	using namespace NPtr;
	using namespace NEncoding;
	using namespace NContainer;
	
	namespace NConcurrency
	{
		struct CDistributedAppCommandLineSpecification::CInternal
		{
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
				CEJSON fp_ParseEJSON(CStr const &_Value, CStr const &_Error, CStr const &_Identifier) const;
				CEJSON fp_ConvertValue(CEJSON const &_Template, CEJSON const &_Value, CStr const &_Identifier, bool _bStrict) const;
				CStr fp_FormatValue(CEJSON const &_Template, CEJSON const &_Value, CStr const &_Identifier) const;
				void fp_ValidateTemplate(CEJSON const &_Template, CStr const &_Identifier, bool _bPrevIsSetOf) const;
				CEJSON f_ConvertValue(CEJSON const &_Value) const;
				void f_AppendConvertValue(CEJSON &o_Value, CEJSON const &_Value) const;
				CStr f_FormatValue(CEJSON const &_Value) const;
				
				CStr const m_Identifier;
				bool const m_bOptional;
				
				CEJSON m_TypeTemplate;
				CEJSON m_Default;
				
				CStr m_ShortDescription;
				CStr m_LongDescription;
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
				
				TCVector<CStr> const m_Names;
				CCommand * const m_pCommand;
				bool m_bDefaultEnabled = true;
				bool m_bHidden = false;
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
				
				TCSet<CStr> m_AllowedSectionOptions;
				TCSet<CStr> m_DisallowedSectionOptions;
				
				NFunction::TCFunction<TCContinuation<CDistributedAppCommandLineResults> (NEncoding::CEJSON const &_Parameters)> m_fActorRunCommand;
				NFunction::TCFunction<uint32 (NEncoding::CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)> m_fDirectRunCommand;
				
				bool m_bErrorOnCommandAsParameter = true;
				bool m_bErrorOnOptionAsParameter = true;
				bool m_bAlwaysVerbose = false;
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
}
