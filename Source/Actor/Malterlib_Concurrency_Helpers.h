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


	DMibImpErrorClass(CExceptionActorAlreadyDestroyed, NMib::NException::CException);
#		define DMibErrorActorAlreadyDestroyed(_Description) DMibImpError(NMib::NException::CExceptionActorAlreadyDestroyed, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorAlreadyDestroyed(_Description) DMibErrorActorAlreadyDestroyed(_Description)
#		endif

	DMibImpErrorClass(CExceptionActorResultWasNotSet, NMib::NException::CException);
#		define DMibErrorActorResultWasNotSet(_Description) DMibImpError(NMib::NException::CExceptionActorResultWasNotSet, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorResultWasNotSet(_Description) DMibErrorActorResultWasNotSet(_Description)
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
		inline_always CCurrentActorScope(CActor const *_pActor);
		inline_always CCurrentActorScope(TCActor<CActor> const &_Actor);
		inline_always ~CCurrentActorScope();
	private:
		CActor *mp_pLastActor;
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
		
#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks *fg_SetConcurrentCallstacks(CAsyncCallstacks *_pCallstacks);
		
		struct CAsyncCallstacksScope
		{
			CAsyncCallstacks *m_pOld;
			CAsyncCallstacksScope(CAsyncCallstacks &_New)
				: m_pOld(NPrivate::fg_SetConcurrentCallstacks(&_New))
			{
				
			}
			~CAsyncCallstacksScope()
			{
				NPrivate::fg_SetConcurrentCallstacks(m_pOld);
			}
		};
#endif
	}
}
