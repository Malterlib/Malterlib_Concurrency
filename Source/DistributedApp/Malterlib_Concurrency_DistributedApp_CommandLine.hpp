// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CCommandLineControl::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_ControlActor);
		_Stream % m_CommandLineWidth;
		_Stream % m_CommandLineHeight;
		_Stream % m_AnsiFlags;
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
