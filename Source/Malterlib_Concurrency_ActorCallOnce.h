// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CResult>
		class TCActorCallOnce
		{
		public:
			TCActorCallOnce(TCWeakActor<CActor> const &_Actor, NFunction::TCFunction<TCContinuation<tf_CResult> (NFunction::CThisTag &)> &&_fFunction);
			~TCActorCallOnce();
			auto operator()();
			
		private:
			struct CCallState : public CActor
			{
				CCallState(TCWeakActor<CActor> const &_Actor, NFunction::TCFunction<TCContinuation<tf_CResult> (NFunction::CThisTag &)> &&_fToPerform);
				TCContinuation<tf_CResult> f_Call();
				
				TCWeakActor<CActor> m_Actor;
				NFunction::TCFunction<TCContinuation<tf_CResult> (NFunction::CThisTag &)> m_fToPerform;
				NContainer::TCVector<TCContinuation<tf_CResult>> m_Continuations;
				bool m_bRunning = false;
				TCAsyncResult<tf_CResult> m_Result;
			};
			
			TCActor<CCallState> m_CallState;
		};
		
	}
}

#include "Malterlib_Concurrency_ActorCallOnce.hpp"