// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	struct CCancellationRootActor : CActor
	{
		explicit CCancellationRootActor(TCPromise<void> &&_Resume);

		~CCancellationRootActor();

		TCPromise<void> m_Resume;
	};

	CCancellationRootActor::CCancellationRootActor(TCPromise<void> &&_Resume)
		: m_Resume(fg_Move(_Resume))
	{
	}

	CCancellationRootActor::~CCancellationRootActor()
	{
		m_Resume.f_SetResult();
	}

	struct CCancelledDelegatedActor : CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		template <typename tf_CResult>
		TCFuture<tf_CResult> f_WaitForDestruction(TCFuture<void> _Resume);
	};

	template <typename tf_CResult>
	TCFuture<tf_CResult> CCancelledDelegatedActor::f_WaitForDestruction(TCFuture<void> _Resume)
	{
		co_await fg_Move(_Resume);

		co_return {};
	}

	struct CDelegatedDestroyState
	{
		NThread::CEventAutoReset m_Progress;
		TCPromise<void> m_Resume;
		NAtomic::TCAtomic<umint> m_nStarted{0};
		NAtomic::TCAtomic<umint> m_nFinished{0};
		NAtomic::TCAtomic<umint> m_nDestructed{0};
		NAtomic::TCAtomic<bool> m_bParentDone{false};
	};

	struct CAsyncDestroyDelegate : CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		CAsyncDestroyDelegate(NStorage::TCSharedPointer<CDelegatedDestroyState> &&_pState, bool _bFail);

		~CAsyncDestroyDelegate();

		TCFuture<void> fp_Destroy() override;

		NStorage::TCSharedPointer<CDelegatedDestroyState> m_pState;
		bool m_bFail;
	};

	CAsyncDestroyDelegate::CAsyncDestroyDelegate(NStorage::TCSharedPointer<CDelegatedDestroyState> &&_pState, bool _bFail)
		: m_pState(fg_Move(_pState))
		, m_bFail(_bFail)
	{
	}

	CAsyncDestroyDelegate::~CAsyncDestroyDelegate()
	{
		m_pState->m_nDestructed.f_FetchAdd(1);
	}

	TCFuture<void> CAsyncDestroyDelegate::fp_Destroy()
	{
		m_pState->m_nStarted.f_FetchAdd(1);
		m_pState->m_Progress.f_Signal();
		co_await m_pState->m_Resume.f_Future();
		m_pState->m_nFinished.f_FetchAdd(1);

		if (m_bFail)
			co_return DMibErrorInstance("Delegated cleanup failed");

		co_return {};
	}

	struct CAsyncDestroyRoot : CActor
	{
		explicit CAsyncDestroyRoot(bool _bFail);

		TCFuture<void> fp_Destroy() override;

		bool m_bFail;
	};

	CAsyncDestroyRoot::CAsyncDestroyRoot(bool _bFail)
		: m_bFail(_bFail)
	{
	}

	TCFuture<void> CAsyncDestroyRoot::fp_Destroy()
	{
		if (m_bFail)
			co_return DMibErrorInstance("Parent cleanup failed");

		co_return {};
	}

	struct CQueuedDelegateDestroyState
	{
		TCPromise<void> m_DestroyEntered;
		TCPromise<void> m_ResumeDestroy;
		NThread::CEventAutoReset m_QueueBlocked;
		NThread::CEventAutoReset m_ResumeQueue;
		NAtomic::TCAtomic<bool> m_bParentDestructed{false};
		NAtomic::TCAtomic<bool> m_bChildDestructed{false};
		NAtomic::TCAtomic<bool> m_bChildOutlivedParent{false};
	};

	struct CQueuedDelegateDestroyRoot : CSeparateThreadActor
	{
		explicit CQueuedDelegateDestroyRoot(NStorage::TCSharedPointer<CQueuedDelegateDestroyState> &&_pState);

		~CQueuedDelegateDestroyRoot();

		TCFuture<void> fp_Destroy() override;

		NStorage::TCSharedPointer<CQueuedDelegateDestroyState> m_pState;
	};

	CQueuedDelegateDestroyRoot::CQueuedDelegateDestroyRoot(NStorage::TCSharedPointer<CQueuedDelegateDestroyState> &&_pState)
		: m_pState(fg_Move(_pState))
	{
	}

	CQueuedDelegateDestroyRoot::~CQueuedDelegateDestroyRoot()
	{
		m_pState->m_bParentDestructed.f_Store(true);
	}

	TCFuture<void> CQueuedDelegateDestroyRoot::fp_Destroy()
	{
		m_pState->m_DestroyEntered.f_SetResult();
		return m_pState->m_ResumeDestroy.f_Future();
	}

	struct CQueuedDestroyDelegate : CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		explicit CQueuedDestroyDelegate(NStorage::TCSharedPointer<CQueuedDelegateDestroyState> &&_pState);

		~CQueuedDestroyDelegate();

		NStorage::TCSharedPointer<CQueuedDelegateDestroyState> m_pState;
	};

	CQueuedDestroyDelegate::CQueuedDestroyDelegate(NStorage::TCSharedPointer<CQueuedDelegateDestroyState> &&_pState)
		: m_pState(fg_Move(_pState))
	{
	}

	CQueuedDestroyDelegate::~CQueuedDestroyDelegate()
	{
		m_pState->m_bChildOutlivedParent.f_Store(m_pState->m_bParentDestructed.f_Load());
		m_pState->m_bChildDestructed.f_Store(true);
	}

	struct CFailingConstructDelegate : CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		CFailingConstructDelegate();
	};

	CFailingConstructDelegate::CFailingConstructDelegate()
	{
		throw DMibErrorInstance("Delegated construction failed");
	}

	struct CCoroutineCancellation_Tests : NTest::CTest
	{
		void f_DoTests();
	};

	void CCoroutineCancellation_Tests::f_DoTests()
	{
		DMibTestSuite("DelegatedParentDestruction")
		{
			struct alignas(64) CAlignedResult
			{
				uint8 m_Data[256] = {};
			};

			auto fCheck = []<typename tf_CResult>(NStr::CStr const &_Case)
				{
					DMibTestPath(_Case);
					TCPromise<void> Resume;
					TCActor<CCancellationRootActor> Root = fg_Construct(fg_TempCopy(Resume));
					TCActor<CCancelledDelegatedActor> Delegated{fg_Construct(), Root};
					TCActor<CCancelledDelegatedActor> Actor{fg_Construct(), Delegated};
					TCPromise<bool> Observed;
					TCFuture<tf_CResult> Future = Actor(&CCancelledDelegatedActor::f_WaitForDestruction<tf_CResult>, Resume.f_Future());
					fg_Dispatch
						(
							Actor
							, []
							{
							}
						)
						.f_CallSync()
					;

					Future.f_OnResultSet
						(
							[&Future, Observed](TCAsyncResult<tf_CResult> &&_Result)
							{
								bool bDeleted = !_Result && NException::fg_ExceptionIsOfType<CExceptionActorDeleted>(_Result.f_GetException());
								Future.f_Clear();
								Observed.f_SetResult(bDeleted);
							}
						)
					;

					fg_Move(Root).f_Destroy().f_CallSync();
					DMibExpectTrue(Observed.f_Future().f_CallSync());
					DMibExpectFalse(Future.f_IsValid());
				}
			;

			fCheck.template operator ()<void>("Void");
			fCheck.template operator ()<CAlignedResult>("Aligned result");

			auto fCheckAsyncDestroy = [](NStr::CStr const &_Case, bool _bAlreadyDestroying, bool _bChildFails, bool _bParentFails)
				{
					DMibTestPath(_Case);
					NStorage::TCSharedPointer<CDelegatedDestroyState> pState = fg_Construct();
					TCActor<CAsyncDestroyRoot> Root = fg_Construct(_bParentFails);
					TCActor<CActor> RootForSync = Root;
					TCActor<CCancelledDelegatedActor> Intermediate{fg_Construct(), Root};
					TCActor<CAsyncDestroyDelegate> Child{fg_Construct(fg_TempCopy(pState), _bChildFails), Intermediate};
					fg_Dispatch
						(
							Child
							, []
							{
							}
						)
						.f_CallSync()
					;

					TCFuture<void> ChildDestroy;
					if (_bAlreadyDestroying)
					{
						ChildDestroy = fg_TempCopy(Child).f_Destroy();
						pState->m_Progress.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);
					}

					TCPromise<TCAsyncResult<void>> Finished;
					fg_Move(Root).f_Destroy().f_OnResultSet
						(
							[pState, Finished](TCAsyncResult<void> &&_Result)
							{
								pState->m_bParentDone.f_Store(true);
								pState->m_Progress.f_Signal();
								Finished.f_SetResult(fg_Move(_Result));
							}
						)
					;

					if (_bAlreadyDestroying)
					{
						DMibExpectNoException
							(
								fg_Dispatch
								(
									RootForSync
									, []
									{
									}
								)
								.f_CallSync()
							)
						;
					}
					else
						pState->m_Progress.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);

					DMibExpect(pState->m_nStarted.f_Load(), ==, 1);
					DMibExpect(pState->m_nFinished.f_Load(), ==, 0);
					DMibExpect(pState->m_nDestructed.f_Load(), ==, 0);
					DMibExpectFalse(pState->m_bParentDone.f_Load());
					DMibExpectException
						(
							(TCActor<CCancelledDelegatedActor>{fg_Construct(), RootForSync})
							, DMibImpExceptionInstance(CExceptionActorIsBeingDestroyed, "Cannot register a delegated actor while its parent is being destroyed")
						)
					;

					pState->m_Resume.f_SetResult();
					auto Result = Finished.f_Future().f_CallSync();
					if (_bChildFails)
						DMibExpectException(Result.f_Get(), DMibErrorInstance("Delegated cleanup failed"));
					else if (_bParentFails)
						DMibExpectException(Result.f_Get(), DMibErrorInstance("Parent cleanup failed"));
					else
						DMibExpectTrue(Result);
					DMibExpect(pState->m_nFinished.f_Load(), ==, 1);
					DMibExpect(pState->m_nDestructed.f_Load(), ==, 1);

					if (_bAlreadyDestroying)
					{
						if (_bChildFails)
							DMibExpectException(fg_Move(ChildDestroy).f_CallSync(), DMibErrorInstance("Delegated cleanup failed"));
						else
							DMibExpectNoException(fg_Move(ChildDestroy).f_CallSync());
					}
				}
			;

			fCheckAsyncDestroy("Async cleanup", false, false, false);
			fCheckAsyncDestroy("Already destroying", true, false, false);
			fCheckAsyncDestroy("Child cleanup failure", false, true, false);
			fCheckAsyncDestroy("In-progress cleanup failure", true, true, false);
			fCheckAsyncDestroy("Parent cleanup failure", false, false, true);

			DMibTestCategory("Failed child construction")
			{
				NStorage::TCSharedPointer<CQueuedDelegateDestroyState> pState = fg_Construct();
				TCActor<CSeparateThreadActor> Root{fg_Construct(), "Failed delegate construction"};
				TCFuture<void> Blocked = fg_Dispatch
					(
						Root
						, [pState]
						{
							pState->m_QueueBlocked.f_Signal();
							pState->m_ResumeQueue.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);
						}
					)
				;
				pState->m_QueueBlocked.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);

				DMibExpectException((TCActor<CFailingConstructDelegate>{fg_Construct(), Root}), DMibErrorInstance("Delegated construction failed"));
				TCFuture<void> Destroyed = fg_Move(Root).f_Destroy();

				pState->m_ResumeQueue.f_Signal();
				fg_Move(Blocked).f_CallSync();
				DMibExpectNoException(fg_Move(Destroyed).f_CallSync());
			};

			DMibTestCategory("Last child reference")
			{
				NStorage::TCSharedPointer<CQueuedDelegateDestroyState> pState = fg_Construct();
				TCActor<CQueuedDelegateDestroyRoot> Root{fg_Construct(fg_TempCopy(pState)), "Delegate destruction"};
				TCFuture<void> Destroyed;
				TCFuture<void> Blocked;
				{
					TCActor<CQueuedDestroyDelegate> Child{fg_Construct(fg_TempCopy(pState)), Root};
					fg_Dispatch
						(
							Child
							, []
							{
							}
						)
						.f_CallSync()
					;
					Destroyed = fg_TempCopy(Root).f_Destroy();
					pState->m_DestroyEntered.f_Future().f_CallSync();

					Blocked = fg_Dispatch
						(
							Root
							, [pState]
							{
								pState->m_QueueBlocked.f_Signal();
								pState->m_ResumeQueue.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);
								pState->m_ResumeDestroy.f_SetResult();
							}
						)
					;
					pState->m_QueueBlocked.f_WaitTimeout(10.0 * NTest::gc_TimeoutMultiplier);
				}

				pState->m_ResumeQueue.f_Signal();
				fg_Move(Blocked).f_CallSync();
				fg_Move(Destroyed).f_CallSync();
				DMibExpectTrue(pState->m_bChildDestructed.f_Load());
				DMibExpectTrue(pState->m_bParentDestructed.f_Load());
				DMibExpectFalse(pState->m_bChildOutlivedParent.f_Load());
			};
		};
	}

	DMibTestRegister(CCoroutineCancellation_Tests, Malterlib::Concurrency);
}
