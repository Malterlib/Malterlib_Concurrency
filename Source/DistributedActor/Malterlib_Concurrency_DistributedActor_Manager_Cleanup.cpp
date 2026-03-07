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
		fp_CleanupUpdateTimer(false);
	}

	void CActorDistributionManagerInternal::fp_CleanupMarkInactive(CHost &_Host)
	{
		if (_Host.m_InactiveHostsLink.f_IsInList())
			return;

		_Host.m_InactiveStopwatch.f_Start();
		m_InactiveHosts.f_Insert(_Host);
		fp_CleanupUpdateTimer(false);
	}

	TCFuture<void> CActorDistributionManagerInternal::fp_PreShutdownCleanupPerform()
	{
		co_return {};
	}

	void CActorDistributionManager::f_PrepareShutdown(fp64 _HostTimeout, fp64 _KillHostsTimeout)
	{
		using namespace NStr;

		auto &Internal = *mp_pInternal;
		Internal.m_HostDaemonTimeout = _HostTimeout;
		Internal.m_HostTimeout = _HostTimeout;
		Internal.fp_CleanupUpdateTimer(true);

		self / [this, _KillHostsTimeout]() -> TCFuture<void>
			{
				auto &Internal = *mp_pInternal;

				co_await fg_Timeout(_KillHostsTimeout);

				if (f_IsDestroyed())
					co_return {};

				Internal.m_bPreShutdown = true;

				while (auto *pHost = Internal.m_Hosts.f_FindAny())
					Internal.fp_DestroyHost(**pHost, nullptr, "Kill host after timeout in prepare shutdown ({} s)"_f << _KillHostsTimeout);

				co_return {};
			}
			> g_DiscardResult
		;
	}

	void CActorDistributionManagerInternal::fp_CleanupUpdateTimer(bool _bForceUpdate)
	{
		if (m_pThis->f_IsDestroyed())
			return;

		if (m_InactiveHosts.f_IsEmpty())
		{
			m_CleanupTimerSubscription.f_Clear();
			m_bCleanupSetup = false;
			return;
		}

		if (m_CleanupTimerSubscription && !_bForceUpdate)
			return;

		if (m_bCleanupSetup && !_bForceUpdate)
			return;

		m_bCleanupSetup = true;

		m_pThis->self / [this]() -> TCFuture<void>
			{
				m_CleanupTimerSubscription = co_await fg_RegisterTimer
					(
						fg_Min(m_HostTimeout, m_HostDaemonTimeout) / 10.0
						, [this]() -> TCFuture<void>
						{
							fp_CleanupPerform();
							co_return {};
						}
					)
				;
				fp_CleanupUpdateTimer(false);

				co_return {};
			}
			> g_DiscardResult
		;
	}

	void CActorDistributionManagerInternal::fp_CleanupPerform()
	{
		using namespace NStr;

		for (auto iHost = m_InactiveHosts.f_GetIterator(); iHost;)
		{
			auto &Host = *iHost;
			++iHost;

			fp64 CleanupTimeout = m_HostTimeout;
			if (m_Enclave.f_IsEmpty() && Host.f_IsDaemon())
				CleanupTimeout = m_HostDaemonTimeout;

			if (Host.m_InactiveStopwatch.f_GetTime() > CleanupTimeout)
				fp_DestroyHost(Host, nullptr, "Host inactivity ({} s)"_f << CleanupTimeout);
		}
	}
}
