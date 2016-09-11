// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp.h"

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActorTrustManagerDatabases/JSONDirectory>
#include <Mib/Concurrency/ActorCallOnce>
#include <Mib/Encoding/Base64>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/String/FuzzyMatch>
#include "Malterlib_Concurrency_DistributedApp_CommandLine_SpecificationInternal.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedAppCommandLineClient::CInternal
		{
			TCActor<CDistributedActorTrustManager> m_TrustManager;
			TCActor<TCDistributedActorSingleSubscription<ICCommandLine>> m_CommandLineSubscription;
			NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> m_pCommandLineSpecification;
			CDistributedAppActor_Settings m_Settings;
			bool m_bInitialized = false;
		};
		
		void CDistributedAppCommandLineClient::f_SetLazyStartApp(NFunction::TCFunction<void (NEncoding::CEJSON const &_Params)> const &_fLazyStartApp)
		{
			mp_fLazyStartApp = _fLazyStartApp;
		}

		aint CDistributedAppCommandLineClient::f_RunCommandLine(NContainer::TCVector<NStr::CStr> const &_CommandLine)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTraceScope;
			auto &Internal = *mp_pInternal;
			
			auto &CommandLineSpec = *(Internal.m_pCommandLineSpecification->mp_pInternal);
			
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
			
			auto fParseValue = [&](CDistributedAppCommandLineSpecification::CInternal::CValue const &_Value, CStr const &_StringData)
				{
					CommandParams[_Value.m_Identifier] = _Value.f_ConvertValue(_StringData);
				}
			;
			
			bool bUsedVectorParam = false;
			
			for (auto &Parameter : _CommandLine)
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
						fParseValue(*pCurrentOption, Parameter);
						pCurrentOption = nullptr;
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
					
					auto fParseOption = [&](CDistributedAppCommandLineSpecification::CInternal::COption const &_Value)
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
								fParseValue(_Value, OptionValue);
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
							if (fParseOption(**pOption))
								continue;
							pCurrentOption = *pOption;
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
							if (fParseOption(**pOption))
								continue;
							pCurrentOption = *pOption;
							continue;
						}
					}
					
					if (auto *pOption = CommandLineSpec.m_GlobalOptionsByName.f_FindEqual(ParsedParameter))
					{
						if (fParseOption(**pOption))
							continue;
						pCurrentOption = *pOption;
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
								Error += fg_Format(". Did you mean any of the following?\n{}", EntryNames);
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
				DMibError(fg_Format("Missing parameter for option: {}", pCurrentOption->m_Names.f_GetFirst()));
			
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
			
			if (iCommandParameter && !iCommandParameter->m_bOptional)
				DMibError(fg_Format("Missing required command parameter: {}", iCommandParameter->m_Identifier));
			
			for (; iCommandParameter; ++iCommandParameter)
			{
				if (iCommandParameter->m_bVector && bUsedVectorParam)
					break;
				if (iCommandParameter->m_Default.f_IsValid())
					CommandParams[iCommandParameter->m_Identifier] = iCommandParameter->m_Default;
			}
				
			return f_RunCommand(pFoundCommand->m_Names.f_GetFirst(), CommandParams);
		}
		
		aint CDistributedAppCommandLineClient::f_RunCommand(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
		{
			auto &Internal = *mp_pInternal;
			auto &CommandLineSpec = *(Internal.m_pCommandLineSpecification->mp_pInternal);
			
			auto pFoundCommand = CommandLineSpec.m_CommandByName.f_FindEqual(_Command);
			if (!pFoundCommand)
				DMibError(fg_Format("Command not found: {}", pFoundCommand));
			
			auto &Command = **pFoundCommand;
			
			if (Command.m_fDirectRunCommand)
				return Command.m_fDirectRunCommand(_Params, *this);
			else if (Command.m_fActorRunCommand)
			{
				if (mp_fLazyStartApp)
					mp_fLazyStartApp(_Params);
				fp_Init();
				auto CommandLineActor = Internal.m_CommandLineSubscription(&TCDistributedActorSingleSubscription<ICCommandLine>::f_GetActor).f_CallSync(10.0);
				
				CDistributedAppCommandLineResults Results = DMibCallActor(CommandLineActor, ICCommandLine::f_RunCommandLine, Command.m_Names.f_GetFirst(), _Params).f_CallSync();
				
				for (auto &Output : Results.m_Output)
				{
					if (Output.m_OutputType == CDistributedAppCommandLineResults::EOutputType_StdErr)
						DMibConErrOutRaw(Output.m_Output);
					else
						DMibConOutRaw(Output.m_Output);
				}

				return Results.m_Status;
			}
			
			return 0;
		}
		
		void CDistributedAppCommandLineClient::f_MutateCommandLineSpecification
			(
				NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutate
			)
		{
			auto &Internal = *mp_pInternal; 
			_fMutate(*Internal.m_pCommandLineSpecification, Internal.m_Settings);
		}		 
		
		CDistributedAppCommandLineClient::CDistributedAppCommandLineClient
			(
				CDistributedAppActor_Settings const &_Settings
				, NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> const &_pCommandLineSpecification
			)
			: mp_pInternal(fg_Construct())
		{
			auto &Internal = *mp_pInternal; 
			Internal.m_pCommandLineSpecification = _pCommandLineSpecification;
			Internal.m_Settings = _Settings;
		}
		
		CDistributedAppCommandLineClient::~CDistributedAppCommandLineClient()
		{
			if (mp_pInternal && mp_pInternal->m_TrustManager)
				mp_pInternal->m_TrustManager->f_BlockDestroy();
		}
		
		void CDistributedAppCommandLineClient::fp_Init()
		{
			auto &Internal = *mp_pInternal; 
			if (!Internal.m_bInitialized)
			{
				Internal.m_TrustManager = 
					fg_ConstructActor<CDistributedActorTrustManager>
					(
						fg_ConstructActor<CDistributedActorTrustManagerDatabase_JSONDirectory>
						(
							fg_Format("{}/CommandLineTrustDatabase.{}", Internal.m_Settings.m_ConfigDirectory, Internal.m_Settings.m_AppName)
						)
						, [](NStr::CStr const &_HostID, NStr::CStr const &_FriendlyName) -> NConcurrency::TCActor<NConcurrency::CActorDistributionManager>
						{
							return fg_ConstructActor<NConcurrency::CActorDistributionManager>(_HostID, _FriendlyName);
						}
						, Internal.m_Settings.m_KeySize
						, Internal.m_Settings.m_ListenFlags
						, Internal.m_Settings.f_GetCompositeFriendlyName() + "_CommandLine"
					)
				;
				Internal.m_TrustManager(&CDistributedActorTrustManager::f_Initialize).f_CallSync(60.0);
				Internal.m_CommandLineSubscription = fg_ConstructActor<TCDistributedActorSingleSubscription<ICCommandLine>>
					(
						"com.malterlib/Concurrency/Commandline"
						, Internal.m_TrustManager(&CDistributedActorTrustManager::f_GetDistributionManager).f_CallSync()
					)
				;
				Internal.m_bInitialized = true;
			}
		}
		
		TCContinuation<CDistributedAppCommandLineClient> CDistributedAppActor::f_GetCommandLineClient()
		{
			return fg_Explicit(CDistributedAppCommandLineClient(mp_Settings, mp_pCommandLineSpec));
		}
		
		CDistributedAppCommandLineClient::CDistributedAppCommandLineClient(CDistributedAppCommandLineClient &&_Other) = default;
		CDistributedAppCommandLineClient &CDistributedAppCommandLineClient::operator =(CDistributedAppCommandLineClient &&_Other) = default;
	}
}
