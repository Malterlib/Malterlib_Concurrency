// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	/*ﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯ*\
	|	Class:				A memory exception										|
	\*_____________________________________________________________________________*/

	DMibImpErrorClass(CExceptionActorDeleted, NMib::NException::CException);
#		define DMibErrorActorDeleted(_Description) DMibImpError(NMib::NException::CExceptionActorDeleted, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorDeleted(_Description) DMibErrorActorDeleted(_Description)
#		endif

	template <typename t_CCallbackSignature, bool _bSupportMultiple, typename t_CExtraData>
	class TCActorSubscriptionManager;

	template <typename t_CActor>
	class TCActorInternal;

	template 
	<
		typename t_CActor
		, typename t_CRet
		, typename t_CFunctor
		, typename t_CResultActor
		, typename t_CResultFunctor
	>
	struct TCReportLocal;

	class CTimerActor;
	class CConcurrentActor;
	class CAnyConcurrentActor;
	class CAnyConcurrentActorLowPrio;

	struct CCurrentActorScope
	{
		inline_always CCurrentActorScope(NConcurrency::CConcurrencyManager &_ConcurrencyManager, CActor const *_pActor);
		inline_always CCurrentActorScope(TCActor<CActor> const &_Actor);
		inline_always ~CCurrentActorScope();
	private:
		CActor *mp_pLastActor;
		NConcurrency::CConcurrencyManager &mp_ConcurrencyManager;
	};
	
	namespace NPrivate
	{
		class CDirectResultActor;
		
		struct CDiscardResultFunctor
		{
			template <typename tf_CResult>
			void operator() (TCAsyncResult<tf_CResult> &&_Result) const volatile
			{
			}
		};
		
		template <typename t_CType>
		struct TCRemoveContinuation
		{
			using CType = t_CType;
		};
		
		template <typename t_CType>
		struct TCRemoveContinuation<TCContinuation<t_CType>>
		{
			using CType = t_CType;
		};
		
#if DMibConcurrencyDebugActorCallstacks
		CAsyncCallstacks *fg_SetConcurrentCallstacks(CConcurrencyManager &_ConcurrencyManager, CAsyncCallstacks *_pCallstacks);
		
		struct CAsyncCallstacksScope
		{
			CAsyncCallstacks *m_pOld;
			CConcurrencyManager &m_ConcurrencyManager;
			CAsyncCallstacksScope(CConcurrencyManager &_ConcurrencyManager, CAsyncCallstacks &_New)
				: m_pOld(NPrivate::fg_SetConcurrentCallstacks(_ConcurrencyManager, &_New))
				, m_ConcurrencyManager(_ConcurrencyManager)
			{
				
			}
			~CAsyncCallstacksScope()
			{
				NPrivate::fg_SetConcurrentCallstacks(m_ConcurrencyManager, m_pOld);
			}
		};
#endif
	}
}
