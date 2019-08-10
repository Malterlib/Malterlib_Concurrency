// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	/*ﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯﾯ*\
	|	Class:				A memory exception										|
	\*_____________________________________________________________________________*/

	DMibImpErrorClassDefine(CExceptionActorDeleted, NMib::NException::CException);
#		define DMibErrorActorDeleted(_Description) DMibImpError(NMib::NException::CExceptionActorDeleted, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorDeleted DMibErrorActorDeleted
#		endif

	DMibImpErrorClassDefine(CExceptionActorAlreadyDestroyed, NMib::NException::CException);
#		define DMibErrorActorAlreadyDestroyed(_Description) DMibImpError(NMib::NException::CExceptionActorAlreadyDestroyed, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorAlreadyDestroyed DMibErrorActorAlreadyDestroyed
#		endif

	DMibImpErrorClassDefine(CExceptionActorIsBeingDestroyed, NMib::NException::CException);
#		define DMibErrorActorIsBeingDestroyed(_Description) DMibImpError(NMib::NException::CExceptionActorIsBeingDestroyed, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorIsBeingDestroyed DMibErrorActorIsBeingDestroyed
#		endif

	DMibImpErrorClassDefine(CExceptionActorResultWasNotSet, NMib::NException::CException);
#		define DMibErrorActorResultWasNotSet(_Description) DMibImpError(NMib::NException::CExceptionActorResultWasNotSet, _Description, false)

#		ifndef DMibPNoShortCuts
#			define DErrorActorResultWasNotSet DMibErrorActorResultWasNotSet
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

	struct CTimerActor;
	struct CConcurrentActor;
	class CActorHolder;

	struct CAnyConcurrentActor;
	struct CAnyConcurrentActorLowPrio;

	struct CCurrentActorScope
	{
		inline_always CCurrentActorScope(CActor const *_pActor);
		inline_always CCurrentActorScope(TCActor<CActor> const &_Actor);
		inline_always ~CCurrentActorScope();

	private:
		DMibThreadLocalScopeDebugMember;
		CActor *mp_pLastActor;
	};

	struct CCurrentlyProcessingActorScope
	{
		CCurrentlyProcessingActorScope(CActor const *_pActor);
		CCurrentlyProcessingActorScope(TCActor<CActor> const &_Actor);
		~CCurrentlyProcessingActorScope();

	private:
		DMibThreadLocalScopeDebugMember;
		CActor *mp_pLastActor;
#if DMibEnableSafeCheck > 0
		CActorHolder *mp_pLastProcessingActorHolder;
#endif
	};

	namespace NPrivate
	{
		struct CDirectResultActor;

		struct CDiscardResultFunctor
		{
			template <typename tf_CResult>
			void operator() (TCAsyncResult<tf_CResult> &&_Result) const volatile
			{
			}
		};
		
		template <typename t_CType>
		struct TCRemoveFuture
		{
			using CType = t_CType;
		};
		
		template <typename t_CType>
		struct TCRemoveFuture<TCFuture<t_CType>>
		{
			using CType = t_CType;
		};
		
#if DMibConfig_Concurrency_DebugActorCallstacks
		CAsyncCallstacks *fg_SetConcurrentCallstacks(CAsyncCallstacks *_pCallstacks, CAsyncCallstacks *_pPredicate);

		struct CAsyncCallstacksScope
		{
			CAsyncCallstacksScope(CAsyncCallstacks &_New)
				: m_pOld(NPrivate::fg_SetConcurrentCallstacks(&_New, nullptr))
				, m_pThis(&_New)
			{
			}

			CAsyncCallstacksScope(CAsyncCallstacksScope const &_Other)
				: m_pOld(_Other.m_pOld)
				, m_pThis(_Other.m_pThis)
			{
			}

			CAsyncCallstacksScope(CAsyncCallstacksScope &&_Other)
				: m_pOld(_Other.m_pOld)
				, m_pThis(_Other.m_pThis)
			{
				_Other.m_bValid = false;
			}

			~CAsyncCallstacksScope()
			{
				if (m_bValid)
					NPrivate::fg_SetConcurrentCallstacks(m_pOld, m_pThis);
			}

			CAsyncCallstacks *m_pOld;
			CAsyncCallstacks *m_pThis;
			bool m_bValid = true;
		};
#endif
	}
}
