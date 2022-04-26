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
			return static_cast<tf_CActor *>(_Actor->fp_GetActorRelaxed());
		}

		template <typename tf_CActor>
		tf_CActor *fg_GetInternalActor(TCActorInternal<tf_CActor> const &_Actor)
		{
			return static_cast<tf_CActor *>(_Actor.fp_GetActorRelaxed());
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
	TCActorInternal<CActor> *TCActorInternal<t_CActor>::fp_GetRealActor(NConcurrency::CActorHolder *_pActorInternal) const
	{
		return t_CActor::fs_GetRealActor(_pActorInternal);
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

		return *NPrivate::fg_GetInternalActor(*this);
	}

	template <typename t_CActor>
	t_CActor const &TCActorInternal<t_CActor>::f_AccessInternal() const
	{
		static_assert(t_CActor::mc_bAllowInternalAccess, "You do not have internal access to this actor");

		return *NPrivate::fg_GetInternalActor(*this);
	}

	template <typename tf_CToDelete>
	void fg_DeleteWeakObject(CInternalActorAllocator &, tf_CToDelete *_pObject)
		requires (NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value)
	{
		bool bImmediateDelete = false;
		if (CActorHolder::fsp_ScheduleActorHolderDestroy(_pObject, bImmediateDelete))
			return;

		if (bImmediateDelete)
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
					, g_OnScopeExit / [_pObject]
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

	template
		<
			typename tf_CActor
			, typename tf_CFunction
			, typename tf_CFunctor
			, typename tf_CResultActor
			, typename tf_CResultFunctor
		>
	bool CActorHolder::f_Call
		(
			tf_CFunctor &&_fToCall
			, TCActor<tf_CResultActor> &&_ResultActor
			, tf_CResultFunctor &&_fResultFunctor
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
			NStr::CStr ToLog = NStr::fg_Format("ReportLocal: {}  {}", sizeof(CReportLocal), alignof(CReportLocal));
			if (s_Logged(ToLog).f_WasCreated())
				DMibTraceSafe("{}\n", ToLog);
		}
#endif
		if constexpr (NTraits::TCRemoveReference<tf_CFunctor>::CType::mc_bDirectCall)
			CReportLocal{fg_Move(_fToCall), fg_Move(_fResultFunctor), fg_Move(_ResultActor), (TCActorInternal<tf_CActor> *)this, false}();
		else
			this->fp_QueueProcess(CReportLocal{fg_Move(_fToCall), fg_Move(_fResultFunctor), fg_Move(_ResultActor), (TCActorInternal<tf_CActor> *)this, true});

		return true; // Dummy return to allow for fg_Swallow on arguments
	}
}
