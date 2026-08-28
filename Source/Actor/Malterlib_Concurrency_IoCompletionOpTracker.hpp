// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	inline void CIoCompletionOpTracker::fp_Acquire()
	{
		m_State.f_FetchAdd(1, NAtomic::gc_MemoryOrder_SequentiallyConsistent);
	}

	inline void CIoCompletionOpTracker::fp_Release()
	{
		uint32 Previous = m_State.f_FetchSub(1, NAtomic::gc_MemoryOrder_SequentiallyConsistent);
		if ((Previous & ~mc_DrainFlag) == 1 && (Previous & mc_DrainFlag))
			(*m_DrainPromise).f_SetResult();
	}

	inline CIoCompletionOpHold::CIoCompletionOpHold(NStorage::TCSharedPointer<CIoCompletionOpTracker> const &_pTracker)
		: mp_pTracker(_pTracker)
	{
		mp_pTracker->fp_Acquire();
	}

	inline CIoCompletionOpHold::CIoCompletionOpHold(CIoCompletionOpHold &&_Other)
		: mp_pTracker(fg_Move(_Other.mp_pTracker))
	{
		_Other.mp_pTracker.f_Clear();
	}

	inline CIoCompletionOpHold::~CIoCompletionOpHold()
	{
		fp_Release();
	}

	inline CIoCompletionOpHold &CIoCompletionOpHold::operator = (CIoCompletionOpHold &&_Other)
	{
		fp_Release();
		mp_pTracker = fg_Move(_Other.mp_pTracker);
		_Other.mp_pTracker.f_Clear();

		return *this;
	}

	inline void CIoCompletionOpHold::fp_Release()
	{
		if (!mp_pTracker)
			return;

		mp_pTracker->fp_Release();
		mp_pTracker.f_Clear();
	}
}
