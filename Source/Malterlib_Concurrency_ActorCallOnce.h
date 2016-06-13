// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/WeakActor>

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CResult, typename ...tp_CParams>
		class TCActorCallOnce
		{
		public:
			TCActorCallOnce
				(
					TCWeakActor<CActor> const &_Actor
					, NFunction::TCFunction<TCContinuation<t_CResult> (NFunction::CThisTag &, tp_CParams...)> &&_fFunction
					, NStr::CStr const &_ErrorOnRunning = NStr::CStr()
				)
			;
			~TCActorCallOnce();
			
			auto operator()(tp_CParams const &...p_Params);
			
		private:
			struct CCallState : public CActor
			{
				CCallState
					(
						TCWeakActor<CActor> const &_Actor
						, NFunction::TCFunction<TCContinuation<t_CResult> (NFunction::CThisTag &, tp_CParams...)> &&_fToPerform
						, NStr::CStr const &_ErrorOnRunning
					)
				;
				TCContinuation<t_CResult> f_Call(tp_CParams const &...p_Params);
				
				TCWeakActor<CActor> m_Actor;
				NFunction::TCFunction<TCContinuation<t_CResult> (NFunction::CThisTag &, tp_CParams...)> m_fToPerform;
				NStr::CStr m_ErrorOnRunning;
				NContainer::TCVector<TCContinuation<t_CResult>> m_Continuations;
				bool m_bRunning = false;
				TCAsyncResult<t_CResult> m_Result;
			};
			
			TCActor<CCallState> m_CallState;
		};
		
	}
}

#include "Malterlib_Concurrency_ActorCallOnce.hpp"