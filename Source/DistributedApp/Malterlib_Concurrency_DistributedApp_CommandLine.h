// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Encoding/EJSON>
#include <Mib/Process/StdIn>

namespace NMib::NConcurrency
{
	struct ICCommandLineControl : public CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x102
		};

		ICCommandLineControl();
		~ICCommandLineControl();

		using FOnInput = NConcurrency::TCActorFunctorWithID<NConcurrency::TCContinuation<void> (NProcess::EStdInReaderOutputType _Type, NStr::CStrSecure const &_Input)>;
		using FOnBinaryInput = NConcurrency::TCActorFunctorWithID
			<
				NConcurrency::TCContinuation<void> (NProcess::EStdInReaderOutputType _Type, NContainer::CSecureByteVector const &_Input, NStr::CStr const &_Error)
			>
		;

		virtual NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;
		virtual NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;

		virtual NConcurrency::TCContinuation<NContainer::CSecureByteVector> f_ReadBinary() = 0;
		virtual NConcurrency::TCContinuation<NStr::CStrSecure> f_ReadLine() = 0;
		virtual NConcurrency::TCContinuation<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) = 0;
		virtual NConcurrency::TCContinuation<void> f_AbortReads() = 0;

		virtual NConcurrency::TCContinuation<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) = 0;
		virtual NConcurrency::TCContinuation<void> f_StdOut(NStr::CStrSecure const &_Output) = 0;
		virtual NConcurrency::TCContinuation<void> f_StdErr(NStr::CStrSecure const &_Output) = 0;
	};

	struct CCommandLineControl
	{
		struct CColors
		{
			static ch8 const ms_Default[];

			static ch8 const ms_StatusNormal[];
			static ch8 const ms_StatusWarning[];
			static ch8 const ms_StatusError[];

			static ch8 const ms_Prompt[];

			static NStr::CStr fs_Foreground16(uint8 _Color);
			static NStr::CStr fs_Background16(uint8 _Color);
			static NStr::CStr fs_Foreground256(uint8 _Color);
			static NStr::CStr fs_Background256(uint8 _Color);
			static NStr::CStr fs_ForegroundRGB(uint8 _Red, uint8 _Green, uint8 _Blue);
			static NStr::CStr fs_BackgroundRGB(uint8 _Red, uint8 _Green, uint8 _Blue);
		};

		NConcurrency::TCContinuation<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) const;
		NConcurrency::TCContinuation<void> f_StdOut(NStr::CStrSecure const &_Output) const;
		NConcurrency::TCContinuation<void> f_StdErr(NStr::CStrSecure const &_Output) const;

		NConcurrency::TCContinuation<void> operator += (NStr::CStrSecure const &_StdOut) const;
		NConcurrency::TCContinuation<void> operator %= (NStr::CStrSecure const &_StdErr) const;
		NConcurrency::TCContinuation<void> operator += (NStr::CStr::CFormat const &_StdOut) const;
		NConcurrency::TCContinuation<void> operator %= (NStr::CStr::CFormat const &_StdErr) const;
		NConcurrency::TCContinuation<void> operator += (NStr::CStrSecure::CFormat const &_StdOut) const;
		NConcurrency::TCContinuation<void> operator %= (NStr::CStrSecure::CFormat const &_StdErr) const;
		NConcurrency::TCContinuation<void> operator += (NContainer::CSecureByteVector const &_StdOut) const;

		NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const;
		NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>>
			f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const
		;
		NConcurrency::TCContinuation<NContainer::CSecureByteVector> f_ReadBinary() const;
		NConcurrency::TCContinuation<NStr::CStrSecure> f_ReadLine() const;
		NConcurrency::TCContinuation<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) const;
		NConcurrency::TCContinuation<void> f_AbortReads() const;

		uint32 f_AddAsyncResult(CAsyncResult const &_Result) const;

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		TCDistributedActorInterfaceWithID<ICCommandLineControl, gc_SubscriptionNotRequired> m_ControlActor;
		uint32 m_CommandLineWidth = 0;
		uint32 m_CommandLineHeight = 0;
	};

	struct ICCommandLine : public CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x102
			, EProtocolVersion = 0x102
		};

		ICCommandLine();

		virtual TCContinuation<uint32> f_RunCommandLine
			(
				 NStr::CStr const &_Command
				 , NEncoding::CEJSON const &_Parameters
				 , CCommandLineControl &&_CommandLine
			) = 0
		;
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
					, NFunction::TCFunction<TCContinuation<uint32> (NEncoding::CEJSON const &_Params, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine)> const &_fRunCommand
					, bool _bRunLocalApp = false
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

		struct CParsedCommandLine
		{
			NStr::CStr m_Command;
			NEncoding::CEJSON m_Params;
		};

		CDistributedAppCommandLineSpecification();
		~CDistributedAppCommandLineSpecification() noexcept;

		CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification const &_Other);
		CDistributedAppCommandLineSpecification(CDistributedAppCommandLineSpecification &&_Other);
		CDistributedAppCommandLineSpecification &operator =(CDistributedAppCommandLineSpecification const &_Other);
		CDistributedAppCommandLineSpecification &operator =(CDistributedAppCommandLineSpecification &&_Other);

		CSection f_GetDefaultSection();
		CSection f_AddSection(NStr::CStr const &_Heading, NStr::CStr const &_Description, NStr::CStr const &_AfterSection = {});
		void f_SetDefaultCommand(CCommand const &_Command);
		void f_SetProgramDescription(NStr::CStr const &_Heading, NStr::CStr const &_Description);
		void f_RegisterGlobalOptions(NEncoding::CEJSON const &_Options);
		CParsedCommandLine f_ParseCommandLine(NContainer::TCVector<NStr::CStr> const &_Params);

	private:
		void fp_SetupDefaultCommands();
		void fp_SetupHelpCommand();

		NPtr::TCUniquePointer<CInternal> mp_pInternal;
	};
}

#include "Malterlib_Concurrency_DistributedApp_CommandLine.hpp"
