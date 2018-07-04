// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/DistributedApp>
#include <Mib/Network/SSL>
#include <Mib/Cryptography/EncryptedStream>
#include <Mib/Encoding/Base64>
#include <Mib/Concurrency/DistributedActor>
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	using namespace NProcess;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCryptography;
	using namespace NStream;
	using namespace NNet;
	using namespace NContainer;

	class CDistributedActorTrustManagerAuthenticationActorPassword : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorPassword(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorPassword();

		TCContinuation<CAuthenticationData> f_RegisterFactor(CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
		TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> f_SignAuthenticationRequest
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, TCSet<CStr> const &_Permissions
				, TCMap<CStr, ICDistributedActorAuthenticationHandler::CChallenge> const &_Challenges
			 	, TCMap<CStr, CAuthenticationData> &&_Factors
			 	, NTime::CTime const &_ExpirationTime
			) override
		;
		TCContinuation<bool> f_VerifyAuthenticationResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) override
		;

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
	};

	CDistributedActorTrustManagerAuthenticationActorPassword::CDistributedActorTrustManagerAuthenticationActorPassword(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager)
		: m_TrustManager(_TrustManager)
	{
	}

	CDistributedActorTrustManagerAuthenticationActorPassword::~CDistributedActorTrustManagerAuthenticationActorPassword() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorPassword::f_RegisterFactor
		(
			CStr const &_UserID
			, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		TCContinuation<CAuthenticationData> Continuation;

		CStdInReaderPromptParams NewPasswordPrompt1;
		NewPasswordPrompt1.m_bPassword = true;
		NewPasswordPrompt1.m_Prompt = "Password        : ";
		_pCommandLine->f_ReadPrompt(NewPasswordPrompt1) > Continuation / [=](CStrSecure &&_NewPassword1) mutable
			{
				CStdInReaderPromptParams NewPasswordPrompt2;
				NewPasswordPrompt2.m_bPassword = true;
				NewPasswordPrompt2.m_Prompt = "Password (again): ";
				_pCommandLine->f_ReadPrompt(NewPasswordPrompt2) > Continuation / [=, NewPassword1 = fg_Move(_NewPassword1)](CStrSecure &&_NewPassword2) mutable
					{
						if (NewPassword1 == _NewPassword2)
						{
							fg_ConcurrentDispatch
								(
									[=]
									{
										CSecureByteVector PrivateKeyData;
										CSecureByteVector PublicKeyData;
										CSSLContext::fs_GenerateKeys(PrivateKeyData, PublicKeyData);

										CSecureByteVector PasswordSalt;
										CSecureByteVector ExpansionSalt;
										NSys::fg_Security_GenerateHighEntropyData(PasswordSalt.f_GetArray(8), 8);
										NSys::fg_Security_GenerateHighEntropyData(ExpansionSalt.f_GetArray(8), 8);
										CKeyExpansion KeyExpansion{_NewPassword2, PasswordSalt, CKeyDerivationSettings_Scrypt{}, ExpansionSalt};

										CBinaryStreamMemory<CBinaryStreamDefault, CSecureByteVector> Stream;
										{
											TCBinaryStream_Encrypted<CBinaryStream *> EncryptedStream{KeyExpansion.f_GetKeyIV(), ESSLDigest_SHA512, KeyExpansion.f_GetHMACKey()};
											EncryptedStream.f_Open(&Stream, NFile::EFileOpen_Write);
											EncryptedStream << PrivateKeyData;
										}
										CByteVector EncryptedPrivateKey = Stream.f_MoveVector();

										CAuthenticationData Result;
										Result.m_Category = EAuthenticationFactorCategory_Knowledge;
										Result.m_Name = "Password";
										Result.m_PublicData["PublicKey"] = CByteVector{PublicKeyData.f_GetArray(), PublicKeyData.f_GetLen()};
										Result.m_PrivateData["PrivateKeyEncrypted"] = EncryptedPrivateKey;
										Result.m_PrivateData["PasswordSalt"] = CByteVector{PasswordSalt.f_GetArray(), PasswordSalt.f_GetLen()};
										Result.m_PrivateData["ExpansionSalt"] = CByteVector{ExpansionSalt.f_GetArray(), ExpansionSalt.f_GetLen()};

										return Result;
									}
								) > Continuation;
							;
						}
						else
							Continuation.f_SetException(DMibErrorInstance("Password mismatch"));
					}
				;
			}
		;
		return Continuation;
	}

	TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> CDistributedActorTrustManagerAuthenticationActorPassword::f_SignAuthenticationRequest
		(
			NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, TCSet<CStr> const &_Permissions
			, TCMap<CStr, ICDistributedActorAuthenticationHandler::CChallenge> const &_Challenges
			, TCMap<CStr, CAuthenticationData> &&_Factors
			, NTime::CTime const &_ExpirationTime
		)
	{
		TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> Continuation;

		CStdInReaderPromptParams PasswordPrompt;
		PasswordPrompt.m_bPassword = true;
		PasswordPrompt.m_Prompt = "Authentication required for '{}'\n{vs}\nPassword: "_f << _Description << _Permissions;
		auto TrustManager = m_TrustManager.f_Lock();
		_pCommandLine->f_ReadPrompt(PasswordPrompt) > Continuation / [=, Factors = fg_Move(_Factors)](CStrSecure &&_Password) mutable
			{
				TCActorResultMap<TCTuple<CStr, CStr>, ICDistributedActorAuthenticationHandler::CResponse> AuthenticationResults;

				for (auto const &Challenge : _Challenges)
				{
					for (auto const &Factor : Factors)
					{
						fg_ConcurrentDispatch
							(
								[Factor, _Password, FactorID = Factors.fs_GetKey(Factor), Challenge, _Permissions, _ExpirationTime]() -> ICDistributedActorAuthenticationHandler::CResponse
								{
									ICDistributedActorAuthenticationHandler::CResponse Response;
									auto *pKey = Factor.m_PrivateData.f_FindEqual("PrivateKeyEncrypted");
									if (!pKey)
										return {};
									auto *pPasswordSalt = Factor.m_PrivateData.f_FindEqual("PasswordSalt");
									if (!pPasswordSalt)
										return {};
									auto *pExpansionSalt = Factor.m_PrivateData.f_FindEqual("ExpansionSalt");
									if (!pExpansionSalt)
										return {};

									CSecureByteVector PasswordSalt = pPasswordSalt->f_Binary();
									CSecureByteVector ExpansionSalt = pExpansionSalt->f_Binary();
									CSecureByteVector EncyptedPrivateKey = pKey->f_Binary();
									CBinaryStreamMemoryPtr<> Stream;
									Stream.f_OpenRead(EncyptedPrivateKey.f_GetArray(), EncyptedPrivateKey.f_GetLen());
									CKeyExpansion KeyExpansion{_Password, PasswordSalt, CKeyDerivationSettings_Scrypt{}, ExpansionSalt};
									NException::CDisableExceptionTraceScope Disable;
									try
									{
										CSecureByteVector PrivateKeyData;
										{
											TCBinaryStream_Encrypted<CBinaryStream *> EncryptedStream{KeyExpansion.f_GetKeyIV(), ESSLDigest_SHA512, KeyExpansion.f_GetHMACKey()};
											EncryptedStream.f_Open(&Stream, NFile::EFileOpen_Read);
											EncryptedStream >> PrivateKeyData;
										}
										// If we come here we know the password is correct, if it wasn't f_Open would have thrown the HMAC mismatch exception
										CSecureByteVector Signature = CSSLContext::fs_SignMessage(Challenge.m_ChallengeData, PrivateKeyData);
										return {_Permissions, Challenge, FactorID, Factor.m_Name, Signature, _ExpirationTime};
									}
									catch (CExceptionCryptography const &_Exception)
									{
										// Anticipated exception: HMAC mismatch means decryption failed. Could be a tampered file, but more likely an incorrect password.
										if (_Exception.f_GetErrorStr() == "HMAC mismatch. The file has been tampered with.")
											return {};
										throw;
									}
								}
							 ) > AuthenticationResults.f_AddResult(TCTuple<CStr, CStr>{_Challenges.fs_GetKey(Challenge), Factors.fs_GetKey(Factor)});
						;
					}
				}
				AuthenticationResults.f_GetResults() > Continuation / [Continuation](TCMap<TCTuple<CStr, CStr>, TCAsyncResult<ICDistributedActorAuthenticationHandler::CResponse>> &&_Results)
					{
						TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse> Results;
						for (auto const &Response : _Results)
						{
							if (!Response)
								DMibLogWithCategory(Malterlib/Concurrency/AuthenticationActorPassword, Info, "Unhandled exception: {}", Response.f_GetExceptionStr());
							if (Response->m_ResponseData.f_IsEmpty())
								continue;
							Results[fg_Get<0>(_Results.fs_GetKey(Response))] = *Response;
							// No early out, must collect all valid results
						}
						Continuation.f_SetResult(Results);
					}
				;
			}
		;
		return Continuation;
	};

	TCContinuation<bool> CDistributedActorTrustManagerAuthenticationActorPassword::f_VerifyAuthenticationResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
	{
		TCContinuation<bool> Continuation;

		if (_Response.m_Challenge ==_Challenge && _Challenge.m_UserID && _Response.m_FactorID)
		{
			auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("PublicKey");
			if (pValue && pValue->f_IsBinary())
			{
				fg_ConcurrentDispatch
					(
						[=, PublicKey = pValue->f_Binary()]
						{
							return CSSLContext::fs_VerifySignature(_Response.m_Challenge.m_ChallengeData, PublicKey, _Response.m_ResponseData);
						}
					) > Continuation;
				;
			}
			else
				Continuation.f_SetResult(false);
		}
		else
			Continuation.f_SetResult(false);

		return Continuation;
	}

	class CDistributedActorTrustManagerAuthenticationActorFactoryPassword : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};
	
	mint CDistributedActorTrustManagerAuthenticationActorFactoryPassword::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactoryPassword::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return CAuthenticationActorInfo{fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorPassword>(_TrustManager), EAuthenticationFactorCategory_Knowledge};
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryPassword);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorPassword_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryPassword);
	}
}
