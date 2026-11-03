// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	inline auto CRunLoop::CReferenceCount::f_Decrease(DIfRefCountDebugging(NStorage::CRefCountDebugReference &_Reference)) const
	{
		// Keep the wake resources alive: another thread may release the last reference
		// and destroy the run loop after the decrement below.
		auto fWake = m_fWakeOnReferenceRelease;
		auto Previous = NStorage::CIntrusiveRefCount::f_Decrease(DIfRefCountDebugging(_Reference));
		if (Previous == 1 && fWake)
			fWake();

		return Previous;
	}
}
