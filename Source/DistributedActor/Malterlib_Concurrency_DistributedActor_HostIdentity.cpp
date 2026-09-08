// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Concurrency/LocalHostIdentity>
#include <Mib/Process/Platform>

namespace NMib::NConcurrency
{
	namespace
	{
		struct CLocalHostIdentityState : public CSubSystem
		{
			NThread::CLowLevelLock m_Lock;
			NContainer::TCVector<TCPromise<CLocalHostIdentity>> m_Waiters;
			TCAsyncResult<CLocalHostIdentity> m_Result;
			bool m_bStarted = false;
			bool m_bResolved = false;
		};

		constinit TCSubSystem<CLocalHostIdentityState, ESubSystemDestruction_BeforeMemoryManager> g_LocalHostIdentity = {DAggregateInit};

		CLocalHostIdentity fg_LookUpLocalHostIdentity()
		{
			return {.m_UserName = NProcess::NPlatform::fg_Process_GetUserName(), .m_ComputerName = NProcess::NPlatform::fg_Process_GetComputerName()};
		}
	}

	NStr::CStr CLocalHostIdentity::f_UserAtComputer() const
	{
		return NStr::fg_Format("{}@{}", m_UserName, m_ComputerName);
	}

	void fg_PrefetchLocalHostIdentity()
	{
		fg_PrefetchLocalHostIdentity(fg_Construct(fg_BlockingActor()));
	}

	void fg_PrefetchLocalHostIdentity(NStorage::TCSharedPointer<CBlockingActorCheckout> const &_pBlockingActorCheckout)
	{
		auto &State = *g_LocalHostIdentity;
		{
			DMibLock(State.m_Lock);
			if (State.m_bStarted)
				return;

			State.m_bStarted = true;
		}

		// The checkout lives until the lookup has answered, so file work dispatched after it gets a
		// blocking actor of its own instead of queueing behind the name service
		auto pBlockingActorCheckout = _pBlockingActorCheckout;
		(
			g_Dispatch(*pBlockingActorCheckout) / []
			{
				return fg_LookUpLocalHostIdentity();
			}
		).f_OnResultSet
			(
				[pBlockingActorCheckout](TCAsyncResult<CLocalHostIdentity> &&_Result)
				{
					auto &State = *g_LocalHostIdentity;

					NContainer::TCVector<TCPromise<CLocalHostIdentity>> Waiters;
					{
						DMibLock(State.m_Lock);
						State.m_Result = fg_Move(_Result);
						State.m_bResolved = true;
						Waiters = fg_Move(State.m_Waiters);
					}

					for (auto &Waiter : Waiters)
						Waiter.f_SetResult(State.m_Result);
				}
			)
		;
	}

	TCFuture<CLocalHostIdentity> fg_GetLocalHostIdentity()
	{
		fg_PrefetchLocalHostIdentity();

		auto &State = *g_LocalHostIdentity;
		TCPromise<CLocalHostIdentity> Promise;
		{
			DMibLock(State.m_Lock);
			if (!State.m_bResolved)
			{
				State.m_Waiters.f_Insert(Promise);
				return Promise.f_Future();
			}
		}

		// The result never changes once resolved, so it is read outside the lock
		Promise.f_SetResult(State.m_Result);

		return Promise.f_Future();
	}

	CLocalHostIdentity fg_GetLocalHostIdentityNow()
	{
		auto &State = *g_LocalHostIdentity;
		{
			DMibLock(State.m_Lock);
			if (State.m_bResolved && State.m_Result)
				return *State.m_Result;
		}

		return fg_LookUpLocalHostIdentity();
	}
}
