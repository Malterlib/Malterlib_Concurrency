// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	using namespace NProcess;
	using namespace NStr;

	class CDistributedActorTrustManagerAuthenticationActorNaive : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorNaive();
		virtual ~CDistributedActorTrustManagerAuthenticationActorNaive();

		TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
	};

	CDistributedActorTrustManagerAuthenticationActorNaive::CDistributedActorTrustManagerAuthenticationActorNaive()
	{
	}

	CDistributedActorTrustManagerAuthenticationActorNaive::~CDistributedActorTrustManagerAuthenticationActorNaive() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorNaive::f_RegisterFactor
		(
			NStr::CStr const &_UserID
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

	class CDistributedActorTrustManagerAuthenticationActorFactoryNaive : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		TCActor<ICDistributedActorTrustManagerAuthenticationActor> operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};
	
	mint CDistributedActorTrustManagerAuthenticationActorFactoryNaive::ms_MakeActive;

	TCActor<ICDistributedActorTrustManagerAuthenticationActor> CDistributedActorTrustManagerAuthenticationActorFactoryNaive::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorNaive>();
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryNaive);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorNaive_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryNaive);
	}
}
