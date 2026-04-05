// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
		this->mp_Destroyed.f_Exchange(3);
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
		requires (NTraits::cIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>)
	{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
		_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b00000010000);
#endif

		bool bImmediateDelete = false;
		if (CActorHolder::fs_ScheduleActorHolderDestroy(_pObject, bImmediateDelete))
			return;

		if (bImmediateDelete)
		{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
			_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b00000100000);
#endif
			NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(_pObject);
			_pObject->m_RefCount.f_WeakSetCapturedDelete(CapturedDelete);
			if (_pObject->m_RefCount.f_WeakDecrease(DIfRefCountDebugging(nullptr)) == 0)
			{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
				_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b00001000000);
#endif
				if (CapturedDelete.m_Size)
					CInternalActorAllocator::f_Free(CapturedDelete.m_pMemory, CapturedDelete.m_Size);
				else
					CInternalActorAllocator::f_FreeNoSize(CapturedDelete.m_pMemory);
			}
		}
		else
		{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
			_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b000'1000'0000);
#endif
			_pObject->f_ConcurrencyManager().f_DispatchOnCurrentThreadOrConcurrentFirst
				(
					_pObject->f_GetPriority()
					, g_OnScopeExit / [_pObject](auto && ...pParams)
					{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
						_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b001'0000'0000);
#endif
						NMemory::CCapturedDelete CapturedDelete = NStorage::fg_DeleteWeakObjectGetCapturedDelete(_pObject);
						_pObject->m_RefCount.f_WeakSetCapturedDelete(CapturedDelete);
						if (_pObject->m_RefCount.f_WeakDecrease(DIfRefCountDebugging(nullptr)) == 0)
						{
#if DMibConfig_RefCountDebugging && DMibConfig_RefCountLeakDebugging
							_pObject->m_RefCount.m_Debug->m_DestroyLocation.f_FetchOr(0b01000000000);
#endif
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
}
