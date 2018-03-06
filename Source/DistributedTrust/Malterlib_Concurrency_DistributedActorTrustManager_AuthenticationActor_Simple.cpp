// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	using namespace NProcess;
	using namespace NStr;

	class CDistributedActorTrustManagerAuthenticationActorSimple : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorSimple();
		virtual ~CDistributedActorTrustManagerAuthenticationActorSimple();

		TCContinuation<CAuthenticationData> f_RegisterFactor(NStr::CStr const &_UserID, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
	};
	
	CDistributedActorTrustManagerAuthenticationActorSimple::CDistributedActorTrustManagerAuthenticationActorSimple()
	{
	}

	CDistributedActorTrustManagerAuthenticationActorSimple::~CDistributedActorTrustManagerAuthenticationActorSimple() = default;

	TCContinuation<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorSimple::f_RegisterFactor
		(
			NStr::CStr const &_UserID
			, NPtr::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		TCContinuation<CAuthenticationData> Continuation;

		CStdInReaderPromptParams QuestionPrompt;
		QuestionPrompt.m_bPassword = false;
		QuestionPrompt.m_Prompt = "Secret question: ";
		_pCommandLine->f_ReadPrompt(QuestionPrompt) > Continuation / [=](CStrSecure &&_Question) mutable
			{
				CStdInReaderPromptParams AnswerPrompt;
				AnswerPrompt.m_bPassword = false;
				AnswerPrompt.m_Prompt = "Correct reply: ";
				_pCommandLine->f_ReadPrompt(AnswerPrompt) > Continuation / [=, Question = fg_Move(_Question)](CStrSecure &&_Reply) mutable
					{
						CAuthenticationData Result;
						Result.m_Category = EAuthenticationFactorCategory_Knowledge;
						Result.m_Name = "Simple";
						Result.m_PublicData["Question"] = Question;
						Result.m_PrivateData["Answer"] = _Reply;
						Continuation.f_SetResult(fg_Move(Result));
					}
				;
			}
		;
		return Continuation;
	}

	class CDistributedActorTrustManagerAuthenticationActorFactorySimple : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		TCActor<ICDistributedActorTrustManagerAuthenticationActor> operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};

	mint CDistributedActorTrustManagerAuthenticationActorFactorySimple::ms_MakeActive;

	TCActor<ICDistributedActorTrustManagerAuthenticationActor> CDistributedActorTrustManagerAuthenticationActorFactorySimple::operator ()
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		return fg_ConstructActor<CDistributedActorTrustManagerAuthenticationActorSimple>();
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactorySimple);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorSimple_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactorySimple);
	}
}
