// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

#include <Mib/Network/Sockets/SSL>

namespace NMib::NConcurrency
{
	void CActorDistributionManagerInternal::fp_CleanupMarkActive(CHost &_Host)
	{
		if (!_Host.m_InactiveHostsLink.f_IsInList())
			return;

		_Host.m_InactiveHostsLink.f_Unlink();
		fp_CleanupUpdateTimer();
	}

	void CActorDistributionManagerInternal::fp_CleanupMarkInactive(CHost &_Host)
	{
		if (_Host.m_InactiveHostsLink.f_IsInList())
			return;

		_Host.m_InactiveClock.f_Start();
		m_InactiveHosts.f_Insert(_Host);
		fp_CleanupUpdateTimer();
	}

	static constexpr double gc_NormalTimeout = 10.0 * 60.0; // 10 minutes
	static constexpr double gc_DaemonTimeout = 4.0 * 60.0 * 60.0; // 4 hours

	void CActorDistributionManagerInternal::fp_CleanupUpdateTimer()
	{
		if (m_pThis->f_IsDestroyed())
			return;

		if (m_InactiveHosts.f_IsEmpty())
		{
			m_CleanupTimerSubscription.f_Clear();
			m_bCleanupSetup = false;
			return;
		}

		if (m_CleanupTimerSubscription)
			return;

		if (m_bCleanupSetup)
			return;

		m_bCleanupSetup = true;

		m_pThis->self / [this]() -> TCFuture<void>
			{
				m_CleanupTimerSubscription = co_await fg_RegisterTimer
					(
						gc_NormalTimeout / 10.0
						, [this]() -> TCFuture<void>
						{
							fp_CleanupPerform();
							co_return {};
						}
					)
				;
				fp_CleanupUpdateTimer();

				co_return {};
			}
			> fg_DiscardResult()
		;
	}

	void CActorDistributionManagerInternal::fp_CleanupPerform()
	{
		using namespace NStr;

		for (auto iHost = m_InactiveHosts.f_GetIter(); iHost;)
		{
			auto &Host = *iHost;
			++iHost;

			fp64 CleanupTimeout = gc_NormalTimeout;
			if (m_Enclave.f_IsEmpty() && Host.f_IsDaemon())
				CleanupTimeout = gc_DaemonTimeout;

			if (Host.m_InactiveClock.f_GetTime() > CleanupTimeout)
				fp_DestroyHost(Host, nullptr, "of host inactivity ({} s)"_f << CleanupTimeout);
		}
	}
}
