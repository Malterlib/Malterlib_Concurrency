// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	constexpr EClientPlatform fg_GetLocalClientPlatform()
	{
#if defined(DPlatformFamily_Windows)
		return EClientPlatform::mc_Windows;
#elif defined(DPlatformFamily_macOS)
		return EClientPlatform::mc_MacOS;
#elif defined(DPlatformFamily_Linux)
		return EClientPlatform::mc_Linux;
#else
		return EClientPlatform::mc_Unknown;
#endif
	}

	template <typename tf_CStream>
	void CCommandLineClientInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Platform;
		_Stream % m_PlatformFamily;
		_Stream % m_Terminal;
		_Stream % m_TerminalProgram;
		_Stream % m_TerminalProgramVersion;
		_Stream % m_ColorTerm;
		_Stream % m_Locale;
		_Stream % m_UTCOffsetSeconds;
		_Stream % m_bClipboardSupported;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CScreenChange::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Width;
		_Stream % m_Height;
		_Stream % m_GlyphWidth;
		_Stream % m_GlyphHeight;
	}

	template <typename tf_CStream>
	void CCommandLineControl::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_ControlActor);
		_Stream % m_CommandLineWidth;
		_Stream % m_CommandLineHeight;
		if (_Stream.f_GetVersion() >= ICCommandLine::EProtocolVersion_SupportGlyphSize)
		{
			_Stream % m_CommandLineGlyphWidth;
			_Stream % m_CommandLineGlyphHeight;
		}
		_Stream % m_AnsiFlags;
		if (_Stream.f_GetVersion() >= ICCommandLine::EProtocolVersion_SupportClientInfo)
			_Stream % m_ClientInfo;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CU2FAuthenticate::CResult::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Signature;
		_Stream % m_AppDigest;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CU2FAuthenticate::CAuthenticationAttempt::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_ChallengeDigest;
		_Stream % m_AppDigest;
		_Stream % m_KeyHandle;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CU2FAuthenticate::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Attempts;
		_Stream % m_Prompt;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CU2FRegister::CResult::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_PublicKey;
		_Stream % m_KeyHandle;
		_Stream % m_AttestationCertificate;
		_Stream % m_Signature;
	}

	template <typename tf_CStream>
	void ICCommandLineControl::CU2FRegister::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_ChallengeDigest;
		_Stream % m_AppDigest;
		_Stream % m_Prompt;
	}
}
