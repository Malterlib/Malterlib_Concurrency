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

	TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> CDistributedActorTrustManagerAuthenticationActorFail::f_SignAuthenticationRequest
		(
			NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, TCSet<CStr> const &_Permissions
			, TCMap<CStr, ICDistributedActorAuthenticationHandler::CChallenge> const &_Challenges
			, TCMap<CStr, CAuthenticationData> &&_Factors
			, NTime::CTime const &_ExpirationTime
		)
	{
		*_pCommandLine += "Fail\nAuthentication failing for '{}'\n{}"_f << _Description << _Permissions;

		TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> Continuation;
		TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse> Results;

		for (auto const &Challenge : _Challenges)
		{
			for (auto const &RegisteredFactor : _Factors)
			{
				ICDistributedActorAuthenticationHandler::CResponse &Response = Results[_Challenges.fs_GetKey(Challenge)];
				Response.m_Challenge = Challenge;
				Response.m_Permissions = _Permissions;
				Response.m_ResponseData.f_Insert((uint8 const *)"FAIL", 5);
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				Response.m_ExpirationTime = _ExpirationTime;
				break;
			}
		}
		Continuation.f_SetResult(fg_Move(Results));
		return Continuation;
	};

	TCContinuation<bool> CDistributedActorTrustManagerAuthenticationActorFail::f_VerifyAuthenticationResponse
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
