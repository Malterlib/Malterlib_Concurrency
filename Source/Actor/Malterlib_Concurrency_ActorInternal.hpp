// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename tf_CActor>
		tf_CActor *fg_GetInternalActor(TCActor<tf_CActor> const &_Actor)
		{
			return _Actor->fp_GetActor();
		}
	}

	template <typename t_CActor>
	inline_always CConcurrencyManager &TCActorInternal<t_CActor>::f_ConcurrencyManager() const
	{
		return *this->mp_pConcurrencyManager;
	}

	template <typename t_CActor>
	inline_always EPriority TCActorInternal<t_CActor>::f_GetPriority() const
	{
		return (EPriority)this->mp_Priority;
	}

	template <typename t_CActor>
	TCActorInternal<CActor> *TCActorInternal<t_CActor>::fp_GetRealActor(TCActorInternal<CActor> *_pActorInternal) const
	{
		return t_CActor::fs_GetRealActor(_pActorInternal);
	}

	template <typename t_CActor>
	t_CActor *TCActorInternal<t_CActor>::fp_GetActor() const
	{
		return static_cast<t_CActor *>(this->mp_pActor.f_Get());
	}

	template <typename t_CActor>
	TCActorInternal<t_CActor> *TCActorInternal<t_CActor>::fp_GetThis()
	{
		return this;
	}

	template <typename t_CActor>
	TCActorInternal<t_CActor>::TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData)
		: CSuper(_pConcurrencyManager, t_CActor::mc_bImmediateDelete, (EPriority)t_CActor::mc_Priority, fg_Move(_pDistributedActorData))
	{
	}

	template <typename t_CActor>
	template <typename tf_CP0>
	TCActorInternal<t_CActor>::TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData, tf_CP0 &&_P0)
		: CSuper(_pConcurrencyManager, t_CActor::mc_bImmediateDelete, (EPriority)t_CActor::mc_Priority, fg_Move(_pDistributedActorData), fg_Forward<tf_CP0>(_P0))
	{
	}

	template <typename t_CActor>
	TCActorInternal<t_CActor>::~TCActorInternal()
	{
		this->fp_DestroyThreaded();
	}

	template <typename t_CActor>
	t_CActor &TCActorInternal<t_CActor>::f_AccessInternal()
	{
		static_assert(t_CActor::mc_bAllowInternalAccess, "You do not have internal access to this actor");

		return *fp_GetActor();
	}

	template <typename t_CActor>
	t_CActor const &TCActorInternal<t_CActor>::f_AccessInternal() const
	{
		static_assert(t_CActor::mc_bAllowInternalAccess, "You do not have internal access to this actor");

		return *fp_GetActor();
	}

	template <typename tf_CToDelete>
	auto fg_DeleteWeakObject(CInternalActorAllocator &, tf_CToDelete *_pObject)
		-> typename TCEnableIf<NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value, void>::CType
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (_pObject == ThreadLocal.m_pCurrentlyDestructingActorHolder)
			return;

		smint Expected = 0;
		if (_pObject->mp_bDestroyed.f_CompareExchangeStrong(Expected, 1))
		{
			DMibFastCheck(!_pObject->f_ImmediateDelete()); // Should have been terminated first
			return _pObject->fp_DestroyActorHolder(nullptr, nullptr);
		}

		if (_pObject->f_ImmediateDelete() || (!ThreadLocal.m_bCurrentlyProcessingInActorHolder && ThreadLocal.m_pThisQueue))
		{
			NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(_pObject);
			_pObject->f_WeakRefCountSetCapturedDelete(CapturedDelete);
			if (_pObject->f_WeakRefCountDecrease(DMibRefcountDebuggingOnly(nullptr)) == 0)
			{
				if (CapturedDelete.m_Size)
					CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
				else
					CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
			}
		}
		else
		{
			_pObject->f_ConcurrencyManager().f_DispatchOnCurrentThreadOrConcurrentFirst
				(
					_pObject->f_GetPriority()
					, g_OnScopeExit > [_pObject]
					{
						NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(_pObject);
						_pObject->f_WeakRefCountSetCapturedDelete(CapturedDelete);
						if (_pObject->f_WeakRefCountDecrease(DMibRefcountDebuggingOnly(nullptr)) == 0)
						{
							if (CapturedDelete.m_Size)
								CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
							else
								CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
						}
					}
				)
			;
		}
	}

	template <typename t_CActor>
	template
		<
			typename tf_CActor
			, typename tf_CFunction
			, typename tf_CFunctor
			, typename tf_CResultActor
			, typename tf_CResultFunctor
		>
	bool TCActorInternal<t_CActor>::f_Call
		(
			tf_CFunctor &&_ToCall
			, TCActor<tf_CResultActor> &&_pResultActor
			, tf_CResultFunctor &&_ResultFunctor
		)
	{
		static_assert(!NTraits::TCIsSame<tf_CActor, NPrivate::CDirectResultActor>::mc_Value, "Cannot be called");
		typedef TCReportLocal
			<
				tf_CActor
				, typename NTraits::TCMemberFunctionPointerTraits<tf_CFunction>::CReturn
				, typename NTraits::TCRemoveReference<tf_CFunctor>::CType
				, tf_CResultActor
				, tf_CResultFunctor
			>
			CReportLocal
		;
#if 0
		static NThread::CMutual s_Lock;
		static NContainer::TCSet<NStr::CStr> s_Logged;
		{
			DMibLock(s_Lock);
			NStr::CStr ToLog = NStr::fg_Format("ReportLocal: {}  {}", sizeof(CReportLocal), NTraits::TCAlignmentOf<CReportLocal>::mc_Value);
			if (s_Logged(ToLog).f_WasCreated())
				DMibTraceSafe("{}\n", ToLog);
		}
#endif
		if constexpr (NTraits::TCRemoveReference<tf_CFunctor>::CType::mc_bDirectCall)
			CReportLocal{fg_Move(_ToCall), fg_Move(_ResultFunctor), fg_Move(_pResultActor), (TCActorInternal<tf_CActor> *)this}();
		else
			this->fp_QueueProcess(CReportLocal{fg_Move(_ToCall), fg_Move(_ResultFunctor), fg_Move(_pResultActor), (TCActorInternal<tf_CActor> *)this});

		return true; // Dummy return to allow for fg_Swallow on arguments
	}
}
