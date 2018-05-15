// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/DistributedApp>
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	using namespace NProcess;
	using namespace NStr;
	using namespace NContainer;

	class CDistributedActorTrustManagerAuthenticationActorNaive : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorNaive(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorNaive();

		TCContinuation<CAuthenticationData> f_RegisterFactor(CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> f_AuthenticateCommand
			(
				NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, TCSet<CStr> const &_Permissions
				, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, TCMap<CStr, CAuthenticationData> &&_Factors
			) override
		;
		TCContinuation<bool> f_VerifyResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) override
		;

		TCWeakActor<CDistributedActorTrustManager> const m_TrustManager;
	};

	CDistributedActorTrustManagerAuthenticationActorNaive::CDistributedActorTrustManagerAuthenticationActorNaive(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager)
		: m_TrustManager(_TrustManager)
	{
	}

	CDistributedActorTrustManagerAuthenticationActorNaive::~CDistributedActorTrustManagerAuthenticationActorNaive() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorNaive::f_RegisterFactor
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
							CAuthenticationData Result;
							Result.m_Category = EAuthenticationFactorCategory_Knowledge;
							Result.m_Name = "Naive";
							Result.m_PrivateData["Password"] = _NewPassword2;
							Continuation.f_SetResult(fg_Move(Result));
						}
						else
							Continuation.f_SetException(DMibErrorInstance("Password mismatch"));
					}
				;
			}
		;
		return Continuation;
	}

	TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> CDistributedActorTrustManagerAuthenticationActorNaive::f_AuthenticateCommand
		(
			NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, TCSet<CStr> const &_Permissions
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
		 	, TCMap<CStr, CAuthenticationData> &&_Factors
		)
	{
		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
		CStdInReaderPromptParams PasswordPrompt;
		PasswordPrompt.m_bPassword = true;
		PasswordPrompt.m_Prompt = "Naive\nAuthentication required for '{}'\n{}\nPassword        : "_f << _Description << _Permissions;
		auto TrustManager = m_TrustManager.f_Lock();
		_pCommandLine->f_ReadPrompt(PasswordPrompt) > Continuation / [=](CStrSecure &&_Password) mutable
			{
				if (_Challenge.m_UserID)
				{
					ICDistributedActorAuthenticationHandler::CResponse Response;

					for (auto const &RegisteredFactor : _Factors)
					{
						if (auto *pValue = RegisteredFactor.m_PrivateData.f_FindEqual("Password"))
						{
							if (pValue->f_String() == _Password)
							{
								Response.m_Challenge = _Challenge;
								Response.m_Permissions = _Permissions;
								Response.m_ResponseData.f_Insert((uint8 const *)_Password.f_GetStr(), _Password.f_GetLen());
								Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
								Response.m_FactorName = RegisteredFactor.m_Name;
								break;
							}
						}
					}

					Continuation.f_SetResult(fg_Move(Response));
				}
				else
					Continuation.f_SetResult(ICDistributedActorAuthenticationHandler::CResponse{});
			}
		;
		return Continuation;
	};

	TCContinuation<bool> CDistributedActorTrustManagerAuthenticationActorNaive::f_VerifyResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
	{
		TCContinuation<bool> Continuation;
		DMibConOut2("CDistributedActorTrustManagerAuthenticationActorNaive::f_VerifyResponse\n");

		if (_Response.m_Challenge ==_Challenge && _Challenge.m_UserID && _Response.m_FactorID)
		{
			bool Result = false;
			if (auto *pValue = _AuthenticationData.m_PrivateData.f_FindEqual("Password"))
				Result = pValue->f_String() ==  CStr(_Response.m_ResponseData.f_GetArray(), _Response.m_ResponseData.f_GetLen());
			Continuation.f_SetResult(Result);
		}
		else
			Continuation.f_SetResult(false);

		return Continuation;
	}

	class CDistributedActorTrustManagerAuthenticationActorFactoryNaive : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};
	
	mint CDistributedActorTrustManagerAuthenticationActorFactoryNaive::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactoryNaive::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return CAuthenticationActorInfo{fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorNaive>(_TrustManager), EAuthenticationFactorCategory_Knowledge};
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryNaive);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorNaive_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryNaive);
	}
}
