// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

namespace NMib::NConcurrency
{
	struct CU2FHelpers
	{
		struct CU2FAuthenticate
		{
			struct CResult
			{
				NCryptography::CHashDigest_SHA256 m_AppDigest;
				NContainer::CSecureByteVector m_Signature;
			};

			struct CAuthenticationAttempt
			{
				NCryptography::CHashDigest_SHA256 m_ChallengeDigest;
				NCryptography::CHashDigest_SHA256 m_AppDigest;
				NContainer::CSecureByteVector m_KeyHandle;
			};

			NContainer::TCVector<CAuthenticationAttempt> m_Attempts; // Tries all of them and returns the first successful one
			NStr::CStr m_Prompt;
		};

		struct CU2FRegister
		{
			struct CResult
			{
				NContainer::CSecureByteVector m_PublicKey;
				NContainer::CSecureByteVector m_KeyHandle;
				NContainer::CSecureByteVector m_AttestationCertificate;
				NContainer::CSecureByteVector m_Signature;
			};

			NCryptography::CHashDigest_SHA256 m_ChallengeDigest;
			NCryptography::CHashDigest_SHA256 m_AppDigest;
			NStr::CStr m_Prompt;
		};

		static TCFuture<CU2FRegister::CResult> fs_Register(CU2FRegister _Params);
		static TCFuture<CU2FAuthenticate::CResult> fs_Authenticate(CU2FAuthenticate _Params);
	};
}
