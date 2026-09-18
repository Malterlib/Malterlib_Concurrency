// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	struct CSequencerLifetimeActor : TCActorSequencerActor<void>
	{
		CSequencerLifetimeActor(TCPromise<void> const &_Deleted)
			: TCActorSequencerActor<void>("Lifetime", 1)
			, m_Deleted(_Deleted)
		{
		}

		~CSequencerLifetimeActor()
		{
			m_Deleted.f_SetResult();
		}

		bool f_DestroyStarted() const
		{
			return f_IsDestroyed();
		}

		TCPromise<void> m_Deleted;
	};

	struct CActorSequencer_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("Lifetime") -> TCFuture<void>
			{
				DMibTestCategory("SlotRetainsActor") -> TCFuture<void>
				{
					TCPromise<void> Deleted;
					TCActor<CSequencerLifetimeActor> Actor = fg_Construct(Deleted);
					TCWeakActor<CSequencerLifetimeActor> Weak = Actor;
					auto Slot = co_await Actor(&CSequencerLifetimeActor::f_Sequence);
					Actor.f_Clear();

					auto Result = co_await Weak(&CSequencerLifetimeActor::f_DestroyStarted).f_Wrap();
					DMibExpectTrue(bool(Result));
					if (Result)
						DMibExpectFalse(*Result);

					co_await fg_DestroySubscription(Slot);
					co_await Deleted.f_Future();
					co_return {};
				};

				DMibTestCategory("DestroyWaitsForSlot") -> TCFuture<void>
				{
					TCPromise<void> Deleted;
					TCActor<CSequencerLifetimeActor> Actor = fg_Construct(Deleted);
					auto Slot = co_await Actor(&CSequencerLifetimeActor::f_Sequence);
					auto Waiting = Actor(&CSequencerLifetimeActor::f_Sequence).f_Call();
					auto nWaiting = co_await Actor(&CSequencerLifetimeActor::f_NumWaiting);
					DMibExpect(nWaiting, ==, umint(1));

					auto Destroy = fg_Move(Actor).f_Destroy();
					auto Result = co_await fg_Move(Waiting).f_Wrap();
					DMibExpectFalse(bool(Result));
					DMibExpectFalse(Deleted.f_IsSet());

					co_await fg_DestroySubscription(Slot);
					co_await fg_Move(Destroy);
					co_await Deleted.f_Future();
					co_return {};
				};

				co_return {};
			};
		}
	};

	DMibTestRegister(CActorSequencer_Tests, Malterlib::Concurrency);
}
