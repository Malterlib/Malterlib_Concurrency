// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Cryptography/EncryptedStream>
#include <Mib/Encoding/Base64>

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_AuthActor.h"

namespace NMib::NConcurrency
{
	using namespace NProcess;
	using namespace NStr;
	using namespace NContainer;
	using namespace NCryptography;
	using namespace NStream;
	using namespace NNetwork;
	using namespace NContainer;

	class CDistributedActorTrustManagerAuthenticationActorPassword : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorPassword(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorPassword();

		TCFuture<CAuthenticationData> f_RegisterFactor(CStr const &_UserID, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
				, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		;
		TCFuture<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
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

	TCFuture<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorPassword::f_RegisterFactor
		(
			CStr const &_UserID
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		CStdInReaderPromptParams NewPasswordPrompt1;
		NewPasswordPrompt1.m_bPassword = true;
		NewPasswordPrompt1.m_Prompt = "Adding Password factor to user. Please provide a new password.\n{}Password        : {}"_f
			<< AnsiEncoding.f_Prompt()
			<< AnsiEncoding.f_Default()
		;
		CStrSecure NewPassword1 = co_await _pCommandLine->f_ReadPrompt(NewPasswordPrompt1);

		CStdInReaderPromptParams NewPasswordPrompt2;
		NewPasswordPrompt2.m_bPassword = true;
		NewPasswordPrompt2.m_Prompt = "{}Password (again): {}"_f
			<< AnsiEncoding.f_Prompt()
			<< AnsiEncoding.f_Default()
		;

		CStrSecure NewPassword2 = co_await _pCommandLine->f_ReadPrompt(NewPasswordPrompt2);
		if (NewPassword1 != NewPassword2)
			co_return DMibErrorInstance("Password mismatch");

		auto AuthenticationData = co_await
			(
				g_ConcurrentDispatch / [=]
				{
					CSecureByteVector PrivateKeyData;
					CSecureByteVector PublicKeyData;
					CPublicCrypto::fs_GenerateKeys(PrivateKeyData, PublicKeyData);

					CSecureByteVector PasswordSalt;
					CSecureByteVector ExpansionSalt;
					NSys::fg_Security_GenerateHighEntropyData(PasswordSalt.f_GetArray(8), 8);
					NSys::fg_Security_GenerateHighEntropyData(ExpansionSalt.f_GetArray(8), 8);
					CKeyExpansion KeyExpansion{NewPassword2, PasswordSalt, CKeyDerivationSettings_Scrypt{}, ExpansionSalt};

					CBinaryStreamMemory<CBinaryStreamDefault, CSecureByteVector> Stream;
					{
						TCBinaryStream_Encrypted<CBinaryStream *> EncryptedStream{KeyExpansion.f_GetKeyIV(), EDigestType_SHA512, KeyExpansion.f_GetHMACKey()};
						EncryptedStream.f_Open(&Stream, NFile::EFileOpen_Write);
						EncryptedStream << PrivateKeyData;
					}
					CSecureByteVector EncryptedPrivateKey = Stream.f_MoveVector();

					CAuthenticationData AuthenticationData;
					AuthenticationData.m_Category = EAuthenticationFactorCategory_Knowledge;
					AuthenticationData.m_Name = "Password";
					AuthenticationData.m_PublicData["PublicKey"] = CByteVector{PublicKeyData.f_GetArray(), PublicKeyData.f_GetLen()};
					AuthenticationData.m_PrivateData["PrivateKeyEncrypted"] = EncryptedPrivateKey.f_ToInsecure();
					AuthenticationData.m_PrivateData["PasswordSalt"] = CByteVector{PasswordSalt.f_GetArray(), PasswordSalt.f_GetLen()};
					AuthenticationData.m_PrivateData["ExpansionSalt"] = CByteVector{ExpansionSalt.f_GetArray(), ExpansionSalt.f_GetLen()};

					return AuthenticationData;
				}
			)
		;

		co_return fg_Move(AuthenticationData);
	}

	TCFuture<ICDistributedActorAuthenticationHandler::CResponse> CDistributedActorTrustManagerAuthenticationActorPassword::f_SignAuthenticationRequest
		(
			NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
			, TCMap<CStr, CAuthenticationData> const &_Factors
		)
	{
		auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

		CStdInReaderPromptParams PasswordPrompt;
		PasswordPrompt.m_bPassword = true;
		PasswordPrompt.m_Prompt = "{}Password: {}"_f
			<< AnsiEncoding.f_Prompt()
			<< AnsiEncoding.f_Default()
		;
		
		auto TrustManager = m_TrustManager.f_Lock();
		if (!TrustManager)
			co_return DMibErrorInstance("No trust manager");

		CStrSecure Password = co_await _pCommandLine->f_ReadPrompt(PasswordPrompt);

		TCActorResultMap<CStr, ICDistributedActorAuthenticationHandler::CResponse> AuthenticationResults;

		for (auto const &Factor : _Factors)
		{
			g_ConcurrentDispatch / [Factor, Password, FactorID = _Factors.fs_GetKey(Factor), _SignedProperties]() mutable -> ICDistributedActorAuthenticationHandler::CResponse
				{
					auto *pKey = Factor.m_PrivateData.f_FindEqual("PrivateKeyEncrypted");
					if (!pKey || !pKey->f_IsBinary())
						return {};
					auto *pPasswordSalt = Factor.m_PrivateData.f_FindEqual("PasswordSalt");
					if (!pPasswordSalt || !pPasswordSalt->f_IsBinary())
						return {};
					auto *pExpansionSalt = Factor.m_PrivateData.f_FindEqual("ExpansionSalt");
					if (!pExpansionSalt || !pExpansionSalt->f_IsBinary())
						return {};

					CSecureByteVector PasswordSalt = pPasswordSalt->f_Binary().f_ToSecure();
					CSecureByteVector ExpansionSalt = pExpansionSalt->f_Binary().f_ToSecure();
					CSecureByteVector EncyptedPrivateKey = pKey->f_Binary().f_ToSecure();

					CBinaryStreamMemoryPtr<> Stream;
					Stream.f_OpenRead(EncyptedPrivateKey.f_GetArray(), EncyptedPrivateKey.f_GetLen());

					CKeyExpansion KeyExpansion{Password, PasswordSalt, CKeyDerivationSettings_Scrypt{}, ExpansionSalt};
					NException::CDisableExceptionTraceScope Disable;
					try
					{
						CSecureByteVector PrivateKeyData;
						{
							TCBinaryStream_Encrypted<CBinaryStream *> EncryptedStream{KeyExpansion.f_GetKeyIV(), EDigestType_SHA512, KeyExpansion.f_GetHMACKey()};
							EncryptedStream.f_Open(&Stream, NFile::EFileOpen_Read);
							EncryptedStream >> PrivateKeyData;
						}
						// If we come here we know the password is correct, if it wasn't f_Open would have thrown the HMAC mismatch exception

						ICDistributedActorAuthenticationHandler::CResponse Response;
						Response.m_FactorID = FactorID;
						Response.m_FactorName = Factor.m_Name;
						Response.m_SignedProperties = _SignedProperties;
						Response.m_Signature = CPublicCrypto::fs_SignMessage(_SignedProperties.f_GetSignatureBytes(), PrivateKeyData);

						return Response;
					}
					catch (CExceptionCryptography const &_Exception)
					{
						// Anticipated exception: HMAC mismatch means decryption failed. Could be a tampered file, but more likely an incorrect password.
						if (_Exception.f_GetErrorStr() == "HMAC mismatch. The file has been tampered with.")
							return {};
						throw;
					}
				}
				> AuthenticationResults.f_AddResult(_Factors.fs_GetKey(Factor));
			;
		}

		auto Results = co_await AuthenticationResults.f_GetResults();

		for (auto const &Response : Results)
		{
			if (!Response || Response->m_Signature.f_IsEmpty())
				continue;
			co_return *Response;
		}

		TCVector<CStr> Exceptions;
		for (auto const &Response : Results)
		{
			if (Response)
				continue;
			Exceptions.f_Insert(Response.f_GetExceptionStr());
		}

		if (Exceptions.f_IsEmpty())
			co_return DMibErrorInstance("Wrong password");
		else
			co_return DMibErrorInstance(CStr::fs_Join(Exceptions, "\n"));
	};

	auto CDistributedActorTrustManagerAuthenticationActorPassword::f_VerifyAuthenticationResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
		-> TCFuture<CVerifyAuthenticationReturn>
	{
		auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("PublicKey");
		if (!pValue || !pValue->f_IsBinary())
			co_return CVerifyAuthenticationReturn{};

		auto SignatureBytes = _Response.m_SignedProperties.f_GetSignatureBytes();

		auto Return = co_await
			(
				g_ConcurrentDispatch / [=, PublicKey = pValue->f_Binary().f_ToSecure()]() -> CVerifyAuthenticationReturn
				{
					CVerifyAuthenticationReturn Return;
					Return.m_bVerified = CPublicCrypto::fs_VerifySignature(SignatureBytes, PublicKey, _Response.m_Signature);
					return Return;
				}
			)
		;

		co_return fg_Move(Return);
	}

	class CDistributedActorTrustManagerAuthenticationActorFactoryPassword : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};
	
	mint CDistributedActorTrustManagerAuthenticationActorFactoryPassword::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactoryPassword::operator ()(TCActor<CDistributedActorTrustManager> const &_TrustManager)
	{
		return CAuthenticationActorInfo{fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorPassword>(_TrustManager), EAuthenticationFactorCategory_Knowledge};
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryPassword);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorPassword_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryPassword);
	}
}
