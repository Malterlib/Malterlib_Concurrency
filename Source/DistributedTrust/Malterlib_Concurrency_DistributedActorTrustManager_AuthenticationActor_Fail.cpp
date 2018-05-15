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

	class CDistributedActorTrustManagerAuthenticationActorFail : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorFail(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorFail();

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
	
	CDistributedActorTrustManagerAuthenticationActorFail::CDistributedActorTrustManagerAuthenticationActorFail(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager)
		: m_TrustManager(_TrustManager)
	{
	}

	CDistributedActorTrustManagerAuthenticationActorFail::~CDistributedActorTrustManagerAuthenticationActorFail() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorFail::f_RegisterFactor
		(
			CStr const &_UserID
			, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		TCContinuation<CAuthenticationData> Continuation;

		CAuthenticationData Result;
		Result.m_Category = EAuthenticationFactorCategory_Possession;
		Result.m_Name = "Fail";
		Continuation.f_SetResult(fg_Move(Result));

		return Continuation;
	}

	TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> CDistributedActorTrustManagerAuthenticationActorFail::f_AuthenticateCommand
		(
			NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, TCSet<CStr> const &_Permissions
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
		 	, TCMap<CStr, CAuthenticationData> &&_Factors
		)
	{
		*_pCommandLine += "Fail\nAuthentication failing for '{}'\n{}"_f << _Description << _Permissions;

		TCContinuation<ICDistributedActorAuthenticationHandler::CResponse> Continuation;
		if (_Challenge.m_UserID)
		{
			auto TrustManager = m_TrustManager.f_Lock();

			ICDistributedActorAuthenticationHandler::CResponse Response;

			for (auto const &RegisteredFactor : _Factors)
			{
				Response.m_Challenge = _Challenge;
				Response.m_Permissions = _Permissions;
				Response.m_ResponseData.f_Insert((uint8 const *)"FAIL", 5);
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				break;
			}
			Continuation.f_SetResult(fg_Move(Response));
		}
		else
			Continuation.f_SetResult(ICDistributedActorAuthenticationHandler::CResponse{});
		return Continuation;
	};

	TCContinuation<bool> CDistributedActorTrustManagerAuthenticationActorFail::f_VerifyResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
	{
		if (_Response.m_Challenge ==_Challenge)
		{
			if (!fg_StrCmp(_Response.m_ResponseData.f_GetArray(), "FAIL"))
			{
				DMibConOut2("Verification succeeded but failed");
				return fg_Explicit(false);
			}
		}
		DMibConOut2("Verification failed");
		return fg_Explicit(false);
	}

	class CDistributedActorTrustManagerAuthenticationActorFactoryFail : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};

	mint CDistributedActorTrustManagerAuthenticationActorFactoryFail::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactoryFail::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return CAuthenticationActorInfo{fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorFail>(_TrustManager), EAuthenticationFactorCategory_Possession};
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryFail);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorFail_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryFail);
	}
}
