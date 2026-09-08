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

	// Starts the process-wide identity lookup on a blocking actor at most once.
	void fg_PrefetchLocalHostIdentity()
	{
		{
			auto &State = *g_LocalHostIdentity;
			DMibLock(State.m_Lock);
			if (State.m_bStarted)
				return;
		}

		fg_PrefetchLocalHostIdentity(fg_Construct(fg_BlockingActor()));
	}

	// Queues the lookup on the supplied actor and retains its checkout until completion.
	void fg_PrefetchLocalHostIdentity(NStorage::TCSharedPointer<CBlockingActorCheckout> const &_pBlockingActorCheckout)
	{
		auto &State = *g_LocalHostIdentity;
		{
			DMibLock(State.m_Lock);
			if (State.m_bStarted)
				return;

			State.m_bStarted = true;
		}

		// Retain the supplied blocking-actor checkout until the lookup completes.
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

	// Starts the lookup if needed and returns its shared result.
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

	// Returns the cached identity when ready; otherwise performs the lookup synchronously on the calling thread.
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
