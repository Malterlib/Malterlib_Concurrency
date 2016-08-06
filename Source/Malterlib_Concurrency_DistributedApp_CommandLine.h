// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Encoding/EJSON>

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedAppCommandLineResults
		{
			enum EOutputType
			{
				EOutputType_StdOut
				, EOutputType_StdErr
			};
			
			struct COutput
			{
				COutput();
				COutput(NStr::CStr const &_Output, EOutputType _OutputType = EOutputType_StdOut);
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);

				EOutputType m_OutputType = EOutputType_StdOut;
				NStr::CStr m_Output;
			};
			
			enum
			{
				EVersion = 0x101
			};

			CDistributedAppCommandLineResults();
			CDistributedAppCommandLineResults(NStr::CStr const &_Output, EOutputType _OutputType = EOutputType_StdOut);
			
			void f_AddStdOut(NStr::CStr const &_Output);
			void f_AddStdErr(NStr::CStr const &_Output);
			void f_SetExitStatus(uint32 _Status);
			
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);
			
			NContainer::TCVector<COutput> m_Output;
			uint32 m_Status = 0;
		};
		
		struct ICCommandLine : public CActor
		{
			virtual TCContinuation<CDistributedAppCommandLineResults> f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Parameters) = 0;
		};
		
		struct CDistributedAppActor;
		struct CDistributedAppCommandLineClient;

		struct COneOf
		{
			inline_always COneOf(NEncoding::CEJSON const &_Config);
			template <typename ...tfp_CParams>
			inline_always COneOf(tfp_CParams const &...p_Config);
			
			inline_always operator NEncoding::CEJSON () &&;
			inline_always operator NEncoding::CEJSON () const &;
			
			NEncoding::CEJSON m_Config;
		};
		
		struct COneOfType
		{
			inline_always COneOfType(NEncoding::CEJSON const &_Config);
			template <typename ...tfp_CParams>
			inline_always COneOfType(tfp_CParams const &...p_Config);
			
			inline_always operator NEncoding::CEJSON () &&;
			inline_always operator NEncoding::CEJSON () const &;
			
			NEncoding::CEJSON m_Config;
		};
		
		struct CDistributedAppCommandLineSpecification
		{
			friend struct CDistributedAppActor;
			friend struct CDistributedAppCommandLineClient;
			struct CInternal;
			struct CSection;
			
			struct CCommand
			{
				friend struct CDistributedAppCommandLineSpecification;
				friend struct CSection;
				
				void f_RegisterOptions(NEncoding::CEJSON const &_Options);
			private:
				CCommand(CInternal *_pInternal, void *_pCommand);
				void *mp_pCommand;
				CInternal *mp_pInternal;
			};

			struct CSection
			{
				friend struct CDistributedAppCommandLineSpecification;
				void f_RegisterSectionOptions(NEncoding::CEJSON const &_Options);

				CCommand f_RegisterCommand
					(
						NEncoding::CEJSON const &_CommandDescription
						, NFunction::TCFunction<TCContinuation<CDistributedAppCommandLineResults> (NEncoding::CEJSON const &_Params)> const &_fRunCommand
					)
				;
				
				CCommand f_RegisterDirectCommand
					(
						NEncoding::CEJSON const &_CommandDescription
						, NFunction::TCFunction<uint32 (NEncoding::CEJSON const &_Parameters, CDistributedAppCommandLineClient &_CommandLineClient)> const &_fRunCommand
					)
				;
				
			private:
				CSection(CInternal *_pInternal, void *_pSection);
				void *mp_pSection;
				CInternal *mp_pInternal;
			};
			
			CDistributedAppCommandLineSpecification();
			~CDistributedAppCommandLineSpecification() noexcept;
			
			CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification const &_Other);
			CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification &&_Other);
			CDistributedAppCommandLineSpecification &operator =(CDistributedAppCommandLineSpecification const &_Other);
			CDistributedAppCommandLineSpecification &operator =(CDistributedAppCommandLineSpecification &&_Other);

			CSection f_GetDefaultSection();
			CSection f_AddSection(NStr::CStr const &_Heading, NStr::CStr const &_Description);
			void f_SetDefaultCommand(CCommand const &_Command);
			void f_SetProgramDescription(NStr::CStr const &_Heading, NStr::CStr const &_Description);
			void f_RegisterGlobalOptions(NEncoding::CEJSON const &_Options);
			
		private:
			void fp_SetupDefaultCommands();
			void fp_SetupHelpCommand();
			
			NPtr::TCUniquePointer<CInternal> mp_pInternal;
		};
	}
}

#include "Malterlib_Concurrency_DistributedApp_CommandLine.hpp"
