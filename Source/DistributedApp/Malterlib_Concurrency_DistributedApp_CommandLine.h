// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Encoding/EJson>
#include <Mib/Process/StdIn>
#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/CommandLine/CommandLine>

namespace NMib::NCommandLine
{
	struct CTableRenderHelper;
}

namespace NMib::NConcurrency
{
	// Host platform of a connected command line client, reported through the command line
	// protocol so applications can follow the client's conventions when the client is remote
	enum class EClientPlatform : uint32
	{
		mc_Unknown = 0
		, mc_Windows
		, mc_MacOS
		, mc_Linux
	};

	constexpr EClientPlatform fg_GetLocalClientPlatform();

	// Facts about the command line client host that the application cannot observe itself when
	// the client is remote: platform, terminal identity from the client's environment, locale and
	// clock offset. Terminal identity is what the in band terminal protocol cannot answer — $TERM
	// and friends are set locally on the client host and do not travel over remote connections.
	struct CCommandLineClientInfo
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		static CCommandLineClientInfo fs_CollectLocal();

		NStr::CStr m_PlatformFamily; // Exact compile time platform family, DMibStringize(DPlatformFamily)
		NStr::CStr m_Terminal; // $TERM
		NStr::CStr m_TerminalProgram; // $TERM_PROGRAM
		NStr::CStr m_TerminalProgramVersion; // $TERM_PROGRAM_VERSION
		NStr::CStr m_ColorTerm; // $COLORTERM
		NStr::CStr m_Locale; // $LC_ALL falling back to $LANG
		EClientPlatform m_Platform = EClientPlatform::mc_Unknown;
		int32 m_UTCOffsetSeconds = 0; // Client local time offset from UTC when the command started
		bool m_bClipboardSupported = false; // The client host has system clipboard access
	};

	struct ICCommandLineControl : public CActor
	{
		enum : uint32
		{
			EProtocolVersion_Min = 0x107

			, EProtocolVersion_SupportScreenChange = 0x108
			, EProtocolVersion_SupportClipboard = 0x109

			, EProtocolVersion_Current = 0x109
		};

		struct CU2FAuthenticate
		{
			struct CResult
			{
				template <typename tf_CStream>
				void f_Stream(tf_CStream &_Stream);

				NCryptography::CHashDigest_SHA256 m_AppDigest;
				NContainer::CSecureByteVector m_Signature;
			};

			struct CAuthenticationAttempt
			{
				template <typename tf_CStream>
				void f_Stream(tf_CStream &_Stream);

				NCryptography::CHashDigest_SHA256 m_ChallengeDigest;
				NCryptography::CHashDigest_SHA256 m_AppDigest;
				NContainer::CSecureByteVector m_KeyHandle;
			};

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NContainer::TCVector<CAuthenticationAttempt> m_Attempts; // Tries all of them and returns the first successful one
			NStr::CStr m_Prompt;
		};

		struct CU2FRegister
		{
			struct CResult
			{
				template <typename tf_CStream>
				void f_Stream(tf_CStream &_Stream);

				NContainer::CSecureByteVector m_PublicKey;
				NContainer::CSecureByteVector m_KeyHandle;
				NContainer::CSecureByteVector m_AttestationCertificate;
				NContainer::CSecureByteVector m_Signature;
			};

			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			NCryptography::CHashDigest_SHA256 m_ChallengeDigest;
			NCryptography::CHashDigest_SHA256 m_AppDigest;
			NStr::CStr m_Prompt;
		};

		struct CScreenChange
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			uint32 m_Width = 0;
			uint32 m_Height = 0;
			uint32 m_GlyphWidth = 0;
			uint32 m_GlyphHeight = 0;
		};

		using FOnInput = NConcurrency::TCActorFunctorWithID<NConcurrency::TCFuture<void> (NProcess::EStdInReaderOutputType _Type, NStr::CStrIO _Input)>;
		using FOnBinaryInput = NConcurrency::TCActorFunctorWithID
			<
				NConcurrency::TCFuture<void> (NProcess::EStdInReaderOutputType _Type, NContainer::CIOByteVector _Input, NStr::CStr _Error)
			>
		;
		using FOnCancel = NConcurrency::TCActorFunctorWithID<NConcurrency::TCFuture<bool> ()>;
		using FOnScreenChange = NConcurrency::TCActorFunctorWithID<NConcurrency::TCFuture<void> (CScreenChange _ScreenChange)>;

		ICCommandLineControl();
		~ICCommandLineControl();

		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(FOnInput _fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdInBinary(FOnBinaryInput _fOnInput, NProcess::EStdInReaderFlag _Flags) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForCancellation(FOnCancel _fOnCancel) = 0;
		virtual NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForScreenChange(FOnScreenChange _fOnScreenChange) = 0;

		virtual NConcurrency::TCFuture<NContainer::CIOByteVector> f_ReadBinary() = 0;
		virtual NConcurrency::TCFuture<NStr::CStrIO> f_ReadLine() = 0;
		virtual NConcurrency::TCFuture<NStr::CStrIO> f_ReadPrompt(NProcess::CStdInReaderPromptParams _Params) = 0;
		virtual NConcurrency::TCFuture<void> f_AbortReads() = 0;

		virtual NConcurrency::TCFuture<void> f_StdOutBinary(NContainer::CIOByteVector _Output) = 0;
		virtual NConcurrency::TCFuture<void> f_StdOut(NStr::CStrIO _Output) = 0;
		virtual NConcurrency::TCFuture<void> f_StdErr(NStr::CStrIO _Output) = 0;

		// System clipboard of the client host, so applications reach the clipboard at the user's
		// seat even when they run remotely
		virtual NConcurrency::TCFuture<void> f_Clipboard_SetText(NStr::CStrIO _Text) = 0;
		virtual NConcurrency::TCFuture<NStr::CStrIO> f_Clipboard_GetText() = 0;

		virtual NConcurrency::TCFuture<CU2FRegister::CResult> f_U2F_Register(CU2FRegister _Register) = 0;
		virtual NConcurrency::TCFuture<CU2FAuthenticate::CResult> f_U2F_Authenticate(CU2FAuthenticate _Authenticate) = 0;
	};

	struct CCommandLineControl
	{
		NConcurrency::TCFuture<void> f_StdOutBinary(NContainer::CSecureByteVector const &_Output) const;
		NConcurrency::TCFuture<void> f_StdOutBinary(NContainer::CByteVector const &_Output) const;
		NConcurrency::TCFuture<void> f_StdOut(NStr::CStrIO const &_Output) const;
		NConcurrency::TCFuture<void> f_StdErr(NStr::CStrIO const &_Output) const;
		NCommandLine::CTableRenderHelper f_TableRenderer() const;
		NCommandLine::CAnsiEncoding f_AnsiEncoding() const;

		NConcurrency::TCFuture<ICCommandLineControl::CU2FRegister::CResult> f_U2F_Register(ICCommandLineControl::CU2FRegister &&_Register);
		NConcurrency::TCFuture<ICCommandLineControl::CU2FAuthenticate::CResult> f_U2F_Authenticate(ICCommandLineControl::CU2FAuthenticate &&_Authenticate);

		void operator += (ch8 const *_pStdOut) const;
		void operator %= (ch8 const *_pStdErr) const;

		void operator += (NStr::CStr const &_StdOut) const;
		void operator %= (NStr::CStr const &_StdErr) const;
		void operator += (NStr::CStr::CFormat const &_StdOut) const;
		void operator %= (NStr::CStr::CFormat const &_StdErr) const;

		void operator += (NStr::CStrSecure const &_StdOut) const;
		void operator %= (NStr::CStrSecure const &_StdErr) const;
		void operator += (NStr::CStrSecure::CFormat const &_StdOut) const;
		void operator %= (NStr::CStrSecure::CFormat const &_StdErr) const;

		void operator += (NContainer::CByteVector const &_StdOut) const;
		void operator += (NContainer::CSecureByteVector const &_StdOut) const;

		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>>
			f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnInput, NProcess::EStdInReaderFlag _Flags) const
		;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForCancellation(ICCommandLineControl::FOnCancel &&_fOnCancel) const;

		// False when the connected command line peer negotiated a protocol without f_RegisterForScreenChange
		bool f_SupportsScreenChange() const;

		// The client's host platform; peers negotiating a protocol without client info fall back
		// to the local platform, which matches the common same-host case. The rest of the client
		// facts are read directly from m_ClientInfo (empty for old peers).
		EClientPlatform f_GetClientPlatform() const;

		// False when the peer negotiated a protocol without clipboard access or its host has no
		// system clipboard
		bool f_SupportsClipboard() const;

		NConcurrency::TCFuture<void> f_Clipboard_SetText(NStr::CStrIO const &_Text) const;
		NConcurrency::TCFuture<NStr::CStrIO> f_Clipboard_GetText() const;
		NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> f_RegisterForScreenChange(ICCommandLineControl::FOnScreenChange &&_fOnScreenChange) const;

		NConcurrency::TCFuture<NContainer::CIOByteVector> f_ReadBinary() const;
		NConcurrency::TCFuture<NStr::CStrIO> f_ReadLine() const;
		NConcurrency::TCFuture<NStr::CStrIO> f_ReadPrompt(NProcess::CStdInReaderPromptParams const &_Params) const;
		NConcurrency::TCFuture<void> f_AbortReads() const;

		uint32 f_AddAsyncResult(CAsyncResult const &_Result) const;

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		TCDistributedActorInterfaceWithID<ICCommandLineControl, gc_SubscriptionNotRequired> m_ControlActor;
		uint32 m_CommandLineWidth = 0;
		uint32 m_CommandLineHeight = 0;
		uint32 m_CommandLineGlyphWidth = 0;
		uint32 m_CommandLineGlyphHeight = 0;
		NCommandLine::EAnsiEncodingFlag m_AnsiFlags = NCommandLine::EAnsiEncodingFlag_None;
		CCommandLineClientInfo m_ClientInfo;
	private:
		static TCFuture<void> fsp_SendStdOutBinary(CCommandLineControl const &_This, uint8 const *_pData, umint _DataLen);
	};

	struct ICCommandLine : public CActor
	{
		enum : uint32
		{
			EProtocolVersion_Min = 0x103

			, EProtocolVersion_SupportGlyphSize = 0x104
			, EProtocolVersion_SupportClientInfo = 0x105

			, EProtocolVersion_Current = 0x105
		};

		ICCommandLine();

		virtual TCFuture<uint32> f_RunCommandLine
			(
				 NStr::CStr _Command
				 , NEncoding::CEJsonSorted _Parameters
				 , CCommandLineControl _CommandLine
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
		, EDistributedAppCommandFlag_DontApplyLogging = DMibBit(2)
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
						NEncoding::CEJsonOrdered &&_CommandDescription
						, NFunction::TCFunctionMovable
						<
							TCFuture<uint32> (NEncoding::CEJsonSorted &&_Params, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine)
						> &&_fRunCommand
						, EDistributedAppCommandFlag _Flags = EDistributedAppCommandFlag_None
					)
				;
				typename t_CCommandLineSpecification::CCommand f_RegisterDirectCommand
					(
						NEncoding::CEJsonOrdered &&_CommandDescription
						, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJsonSorted &&_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
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
						TCActorFunctor<TCFuture<uint32> (NEncoding::CEJsonSorted _Parameters, NStorage::TCSharedPointer<CCommandLineControl> _pCommandLine)>
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
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable
			<
				TCFuture<uint32> (NEncoding::CEJsonSorted &&_Params, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine)
			> &&_fRunCommand
			, EDistributedAppCommandFlag _Flags
		)
		-> NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>::CCommand
	;

	extern template auto
	CCommandLineSpecificationDistributedAppCustomization::TCSection<NCommandLine::TCCommandLineSpecification<CCommandLineSpecificationDistributedAppCustomization>>::CSection::
	f_RegisterDirectCommand
		(
			NEncoding::CEJsonOrdered &&_CommandDescription
			, NFunction::TCFunctionMovable<uint32 (NEncoding::CEJsonSorted &&_Parameters, CCommandLineClient &_CommandLineClient)> &&_fRunCommand
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
