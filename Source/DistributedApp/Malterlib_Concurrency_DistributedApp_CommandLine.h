// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Encoding/EJSON>
#include <Mib/Process/StdIn>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/CommandLine/CommandLine>

namespace NMib::NCommandLine
{
	struct CTableRenderHelper;
}

namespace NMib::NConcurrency
{
	struct ICCommandLineControl : public CActor
	{
		enum : uint32
		{
			EProtocolVersion_Min = 0x105
			, EProtocolVersion_Current = 0x105
		};

		ICCommandLineControl();
		~ICCommandLineControl();

		using FOnInput = NConcurrency::TCActorFunctorWithID<NConcurrency::TCFuture<void> (NProcess::EStdInReaderOutputType _Type, NStr::CStrSecure const &_Input)>;
		using FOnBinaryInput = NConcurrency::TCActorFunctorWithID
			<
				NConcurrency::TCFuture<void> (NProcess::EStdInReaderOutputType _Type, NContainer::CSecureByteVector const &_Input, NStr::CStr const &_Error)
			>
		;
		using FOnCancel = NConcurrency::TCActorFunctorWithID<NConcurrency::TCFuture<void> ()>;

		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForCancellation(FOnCancel &&_fOnCancel) = 0;

		virtual NConcurrency::TCFuture<NContainer::CSecureByteVector> f_ReadBinary() = 0;
		virtual NConcurrency::TCFuture<NStr::CStrSecure> f_ReadLine() = 0;
		virtual NConcurrency::TCFuture<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) = 0;
		virtual NConcurrency::TCFuture<void> f_AbortReads() = 0;

		virtual NConcurrency::TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) = 0;
		virtual NConcurrency::TCFuture<void> f_StdOut(NStr::CStrSecure const &_Output) = 0;
		virtual NConcurrency::TCFuture<void> f_StdErr(NStr::CStrSecure const &_Output) = 0;
	};

	struct CCommandLineControl
	{
		NConcurrency::TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) const;
		NConcurrency::TCFuture<void> f_StdOut(NStr::CStrSecure const &_Output) const;
		NConcurrency::TCFuture<void> f_StdErr(NStr::CStrSecure const &_Output) const;
		NCommandLine::CTableRenderHelper f_TableRenderer() const;
		NCommandLine::CAnsiEncoding f_AnsiEncoding() const;

		void operator += (NStr::CStrSecure const &_StdOut) const;
		void operator %= (NStr::CStrSecure const &_StdErr) const;
		void operator += (NStr::CStr::CFormat const &_StdOut) const;
		void operator %= (NStr::CStr::CFormat const &_StdErr) const;
		void operator += (NStr::CStrSecure::CFormat const &_StdOut) const;
		void operator %= (NStr::CStrSecure::CFormat const &_StdErr) const;
		void operator += (NContainer::CSecureByteVector const &_StdOut) const;

		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>>
			f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const
		;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForCancellation(ICCommandLineControl::FOnCancel &&_fOnCancel) const;

		NConcurrency::TCFuture<NContainer::CSecureByteVector> f_ReadBinary() const;
		NConcurrency::TCFuture<NStr::CStrSecure> f_ReadLine() const;
		NConcurrency::TCFuture<NStr::CStrSecure> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) const;
		NConcurrency::TCFuture<void> f_AbortReads() const;

		uint32 f_AddAsyncResult(CAsyncResult const &_Result) const;

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		TCDistributedActorInterfaceWithID<ICCommandLineControl, gc_SubscriptionNotRequired> m_ControlActor;
		uint32 m_CommandLineWidth = 0;
		uint32 m_CommandLineHeight = 0;
		NCommandLine::EAnsiEncodingFlag m_AnsiFlags = NCommandLine::EAnsiEncodingFlag_None;
	};

	struct ICCommandLine : public CActor
	{
		enum : uint32
		{
			EProtocolVersion_Min = 0x103
			, EProtocolVersion_Current = 0x103
		};

		ICCommandLine();

		virtual TCFuture<uint32> f_RunCommandLine
			(
				 NStr::CStr const &_Command
				 , NEncoding::CEJSON const &_Parameters
				 , CCommandLineControl &&_CommandLine
			) = 0
		;
	};

	struct CDistributedAppActor;
	struct CDistributedAppCommandLineClient;

	enum EDistributedAppCommandFlag
	{
		EDistributedAppCommandFlag_None = 0
		, EDistributedAppCommandFlag_RunLocalApp = DMibBit(0)
		, EDistributedAppCommandFlag_WaitForRemotes = DMibBit(1)
	};

	struct CCommandLineSpecificationDistributedAppCustomization
	{
		using CCommandLineClient = CDistributedAppCommandLineClient;
		
		template <typename t_CCommandLineSpecification>
		struct TCSection
		{
			struct CSection : public t_CCommandLineSpecification::CSectionCommon
			{
				friend t_CCommandLineSpecification;
				using t_CCommandLineSpecification::CSectionCommon::CSectionCommon;

				typename t_CCommandLineSpecification::CCommand f_RegisterCommand
					(
						NEncoding::CEJSON const &_CommandDescription
						, NFunction::TCFunctionMovable
						<
							TCFuture<uint32> (NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
						> &&_fRunCommand
						, EDistributedAppCommandFlag _Flags = EDistributedAppCommandFlag_None
					)
				;
				typename t_CCommandLineSpecification::CCommand f_RegisterDirectCommand
					(
						NEncoding::CEJSON const &_CommandDescription
						, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJSON const &_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
						, EDistributedAppCommandFlag _Flags = EDistributedAppCommandFlag_None
					)
				;
			};
		};

		template <typename t_CCommand>
		struct TCInternalCommand
		{
			struct CCommand : public t_CCommand
			{
				using t_CCommand::t_CCommand;

				NStorage::TCSharedPointer
					<
						TCActorFunctor<TCFuture<uint32> (NEncoding::CEJSON const &_Parameters, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)>
					>
					m_pActorRunCommand
				;
				EDistributedAppCommandFlag m_Flags = EDistributedAppCommandFlag_None;
			};
		};
	};

	using CDistributedAppCommandLineSpecification = NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>;

	extern template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterCommand
		(
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJSON const &_Params, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;

	extern template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterDirectCommand
		(
			NEncoding::CEJSON const &_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJSON const &_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;
}

namespace NMib::NCommandLine
{
	extern template struct TCCommandLineSpecification<NConcurrency::CCommandLineSpecificationDistributedAppCustomization>;
}

#include "Malterlib_Concurrency_DistributedApp_CommandLine.hpp"
