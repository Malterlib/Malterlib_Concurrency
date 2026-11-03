// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_Manager.h"

namespace NMib::NConcurrency::NPrivate
{
	void CDummyRunLoop::f_Wait()
	{
		m_WaitEvent.f_Wait();
	}

	bool CDummyRunLoop::f_WaitTimeout(fp64 _Timeout)
	{
		return m_WaitEvent.f_WaitTimeout(_Timeout);
	}

	void CDummyRunLoop::f_Finished()
	{
		m_WaitEvent.f_SetSignaled();
	}

	CRunLoopState::CRunLoopState(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop)
		: m_pManualRunLoop(_pRunLoop)
	{
	}

	void CRunLoopState::f_Wait()
	{
		while (!m_bFinished.f_Load())
			m_pManualRunLoop->f_WaitOnce();
	}

	bool CRunLoopState::f_WaitTimeout(fp64 _Timeout)
	{
		NTime::CStopwatch Stopwatch(true);
		while (!m_bFinished.f_Load())
		{
			fp64 ToWait = _Timeout - Stopwatch.f_GetTime();
			if (ToWait < 0.0)
				return true;

			if (m_pManualRunLoop->f_WaitOnceTimeout(ToWait))
				return true;
		}

		return false;
	}

	void CRunLoopState::f_Finished()
	{
		m_bFinished = true;
		m_pManualRunLoop->f_Wake();
	}
}
