// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/RunLoop>

namespace NMib::NConcurrency
{
	CRunLoop::~CRunLoop() = default;

	void CRunLoop::f_Sleep(fp64 _Time)
	{
		NTime::CClock Clock{true};
		while (true)
		{
			auto WaitTime = _Time - Clock.f_GetTime();
			if (WaitTime <= 0)
				break;

			f_WaitOnceTimeout(WaitTime);
		}

		f_Process();
	}

	void CDefaultRunLoop::f_Process()
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(!m_bProcessing); // Recursive processing is not safe
		m_bProcessing = true;
		auto Cleanup = g_OnScopeExit / [&]
			{
				m_bProcessing = false;
			}
		;
#endif

		bool bDoneSomething = true;
		while (bDoneSomething)
		{
			bDoneSomething = false;

			if (m_RunQueue.f_TransferThreadSafeQueue(m_RunQueueLocal))
				bDoneSomething = true;

			while (auto pEntry = m_RunQueue.f_FirstQueueEntry(m_RunQueueLocal))
			{
				(*pEntry)();
				m_RunQueue.f_PopQueueEntry(pEntry);
				bDoneSomething = true;
			}
		}
	}

	void CDefaultRunLoop::f_WaitOnce()
	{
		f_Process();

		m_Event.f_Wait();
	}

	bool CDefaultRunLoop::f_WaitOnceTimeout(fp64 _Timeout)
	{
		f_Process();

		return m_Event.f_WaitTimeout(_Timeout);
	}

	void CDefaultRunLoop::f_Wake()
	{
		m_Event.f_Signal();
	}

	CActorDestroyEventLoop CDefaultRunLoop::f_ActorDestroyLoop()
	{
		auto pThis = NStorage::TCSharedPointer<CDefaultRunLoop>(fg_Explicit(this));
		return
			{
				[pThis]
				{
					pThis->f_WaitOnce();
				}
				,
				[pThis]
				{
					pThis->f_Wake();
				}
			}
		;
	}

	NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> CDefaultRunLoop::f_Dispatcher()
	{
		return [pThis = NStorage::TCSharedPointer<CDefaultRunLoop>(fg_Explicit(this))](FActorQueueDispatch &&_Dispatch)
			{
				pThis->m_RunQueue.f_AddToQueue(fg_Move(_Dispatch));
				pThis->m_Event.f_Signal();
			}
		;
	}
}
