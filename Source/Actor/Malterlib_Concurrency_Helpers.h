// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	DMibImpErrorClassDefine(CExceptionActorDeleted, NMib::NException::CException);
#		define DMibErrorActorDeleted(_Description) DMibImpError(NMib::NConcurrency::CExceptionActorDeleted, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorDeleted DMibErrorActorDeleted
#		endif

	DMibImpErrorClassDefine(CExceptionActorAlreadyDestroyed, NMib::NException::CException);
#		define DMibErrorActorAlreadyDestroyed(_Description) DMibImpError(NMib::NConcurrency::CExceptionActorAlreadyDestroyed, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorAlreadyDestroyed DMibErrorActorAlreadyDestroyed
#		endif

	DMibImpErrorClassDefine(CExceptionActorIsBeingDestroyed, NMib::NException::CException);
#		define DMibErrorActorIsBeingDestroyed(_Description) DMibImpError(NMib::NConcurrency::CExceptionActorIsBeingDestroyed, _Description)

#		ifndef DMibPNoShortCuts
#			define DErrorActorIsBeingDestroyed DMibErrorActorIsBeingDestroyed
#		endif

	template <typename t_CActor>
	class TCActorInternal;

	struct CTimerActor;
	struct CConcurrentActor;
	class CActorHolder;

	struct CThisConcurrentActor;
	struct CThisConcurrentActorLowPrio;
	struct COtherConcurrentActor;
	struct COtherConcurrentActorLowPrio;

	struct CCurrentActorScope
	{
		inline_always CCurrentActorScope(CConcurrencyThreadLocal &_ThreadLocal, CActorHolder *_pActor);
		inline_always CCurrentActorScope(CConcurrencyThreadLocal &_ThreadLocal, TCActor<CActor> const &_Actor);
		inline_always ~CCurrentActorScope();

	private:
		DMibThreadLocalScopeDebugMember;
		CActorHolder *mp_pLastActor;
		CConcurrencyThreadLocal &mp_ThreadLocal;
		bool mp_bLastProcessing;
	};

	struct CCurrentlyProcessingActorScope
	{
		CCurrentlyProcessingActorScope(CConcurrencyThreadLocal &_ThreadLocal, CActorHolder *_pActor);
		CCurrentlyProcessingActorScope(CConcurrencyThreadLocal &_ThreadLocal, TCActor<CActor> const &_Actor);
		~CCurrentlyProcessingActorScope();

	private:
		DMibThreadLocalScopeDebugMember;
		CActorHolder *mp_pLastActor;
		CConcurrencyThreadLocal &mp_ThreadLocal;
		bool mp_bLastProcessing;
	};

	namespace NPrivate
	{
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
			DMibThreadLocalScopeDebugMember;
		};
#endif
	}
}
