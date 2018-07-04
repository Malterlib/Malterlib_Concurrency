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

	class CDistributedActorTrustManagerAuthenticationActorSucceed : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorSucceed(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorSucceed();

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

	CDistributedActorTrustManagerAuthenticationActorSucceed::CDistributedActorTrustManagerAuthenticationActorSucceed(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager)
		: m_TrustManager(_TrustManager)
	{
	}

	CDistributedActorTrustManagerAuthenticationActorSucceed::~CDistributedActorTrustManagerAuthenticationActorSucceed() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorSucceed::f_RegisterFactor
		(
			CStr const &_UserID
			, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		TCContinuation<CAuthenticationData> Continuation;

		CAuthenticationData Result;
		Result.m_Category = EAuthenticationFactorCategory_Possession;
		Result.m_Name = "Succeed";
		Continuation.f_SetResult(fg_Move(Result));

		return Continuation;
	}

	TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> CDistributedActorTrustManagerAuthenticationActorSucceed::f_SignAuthenticationRequest
		(
			NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, TCSet<CStr> const &_Permissions
			, TCMap<CStr, ICDistributedActorAuthenticationHandler::CChallenge> const &_Challenges
			, TCMap<CStr, CAuthenticationData> &&_Factors
			, NTime::CTime const &_ExpirationTime
		)
	{
		*_pCommandLine += "Succeed\nAuthentication succeeding for '{}'\n{}"_f << _Description << _Permissions;

		TCContinuation<TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse>> Continuation;
		TCMap<CStr, ICDistributedActorAuthenticationHandler::CResponse> Results;
		for (auto const &Challenge : _Challenges)
		{
			for (auto const &RegisteredFactor : _Factors)
			{
				ICDistributedActorAuthenticationHandler::CResponse &Response = Results[_Challenges.fs_GetKey(Challenge)];
				Response.m_Challenge = Challenge;
				Response.m_Permissions = _Permissions;
				Response.m_ResponseData.f_Insert((uint8 const *)"SUCCEED", 8);
				Response.m_FactorID = _Factors.fs_GetKey(RegisteredFactor);
				Response.m_FactorName = RegisteredFactor.m_Name;
				Response.m_ExpirationTime = _ExpirationTime;
				break;
			}
		}
		Continuation.f_SetResult(fg_Move(Results));
		return Continuation;
	};

	TCContinuation<bool> CDistributedActorTrustManagerAuthenticationActorSucceed::f_VerifyAuthenticationResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
	{
		return fg_Explicit(_Response.m_Challenge ==_Challenge && !fg_StrCmp(_Response.m_ResponseData.f_GetArray(), "SUCCEED"));
	}

	class CDistributedActorTrustManagerAuthenticationActorFactorySucceed: public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};

	mint CDistributedActorTrustManagerAuthenticationActorFactorySucceed::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactorySucceed::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return CAuthenticationActorInfo{fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorSucceed>(_TrustManager), EAuthenticationFactorCategory_Possession};
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySucceed);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorSucceed_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactorySucceed);
	}
}

