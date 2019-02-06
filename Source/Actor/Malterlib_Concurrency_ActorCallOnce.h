// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/WeakActor>

namespace NMib::NConcurrency
{
	template <typename t_CResult, typename ...tp_CParams>
	class TCActorCallOnce
	{
	public:
		TCActorCallOnce
			(
				TCActor<CActor> const &_Actor
				, NFunction::TCFunctionMovable<TCFuture<t_CResult> (tp_CParams...)> &&_fFunction
				, bool _bSupportRetry
				, NStr::CStr const &_ErrorOnRunning = NStr::CStr()
			)
		;
		~TCActorCallOnce();

		TCFuture<t_CResult> operator()(tp_CParams const &...p_Params);

	private:
		struct CCallState : public CActor
		{
			using CActorHolder = CDelegatedActorHolder;

			CCallState
				(
					NFunction::TCFunctionMovable<TCFuture<t_CResult> (tp_CParams...)> &&_fToPerform
					, NStr::CStr const &_ErrorOnRunning
					, bool _bSupportRetry
				)
			;
			TCFuture<t_CResult> f_Call(tp_CParams const &...p_Params);

			NFunction::TCFunctionMovable<TCFuture<t_CResult> (tp_CParams...)> m_fToPerform;
			NStr::CStr m_ErrorOnRunning;
			NContainer::TCVector<TCPromise<t_CResult>> m_Promises;
			bool m_bRunning = false;
			bool m_bSupportRetry = false;
			TCAsyncResult<t_CResult> m_Result;
		};

		TCActor<CCallState> m_CallState;
	};
}

#include "Malterlib_Concurrency_ActorCallOnce.hpp"
