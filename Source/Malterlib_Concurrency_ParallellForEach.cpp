// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_ParallellForEach.h"

namespace NMib::NConcurrency
{
	CThreadPool::CThreadPool(mint _nThreads)
		: m_Semaphore(_nThreads)
	{
#if 1
		mint nNumaNodes = NSys::fg_Mem_GetNumNumaNodes();
		NContainer::TCVector<ENumaNode> NumaNodes;
		if (nNumaNodes > 1)
		{
			NumaNodes.f_SetLen(nNumaNodes);
			NSys::fg_Mem_GetNumaNodes(NumaNodes.f_GetArray(), nNumaNodes);

			mint iNode = NMisc::fg_GetRandomUnsigned() % NumaNodes.f_GetLen(); // Randomize
			iNode = 0;
			fg_GetSys()->f_MemoryManager_SetNumaNode(NumaNodes[iNode]); // Set main thread to one specific numa node
			NSys::fg_Thread_SetNumaAffinity(NSys::fg_Thread_GetCurrent(), NumaNodes[iNode]);
		}
		//NumaNodes.f_SetLen(1);
#else

		mint nNumaNodes;
#endif

		if (NumaNodes.f_IsEmpty())
			NumaNodes.f_Insert(ENumaNode_Default);
		nNumaNodes = NumaNodes.f_GetLen();

		fp64 ThreadsPerNumaNode = fp64(_nThreads) / fp64(nNumaNodes);
		fp64 nNumaNodesUsedFP = 0.0;
		aint nNumaNodesUsed = 0;

		for (auto iNumaNode = NumaNodes.f_GetIterator(); iNumaNode; ++iNumaNode)
		{
			nNumaNodesUsedFP += ThreadsPerNumaNode;

			aint nThreads = (nNumaNodesUsedFP - nNumaNodesUsed).f_ToIntRound();

			nNumaNodesUsed += nThreads;

			auto NumaNode = *iNumaNode;

			while (nThreads > 0)
			{
				--nThreads;
				m_Threads.f_Insert
					(
						NThread::CThreadObject::fs_StartThread
						(
							[this, NumaNode](NThread::CThreadObject * _pThread) -> aint
							{
								fg_GetSys()->f_MemoryManager_SetNumaNode(NumaNode);
								NSys::fg_Thread_SetNumaAffinity(NSys::fg_Thread_GetCurrent(), NumaNode);

								while (_pThread->f_GetState() != NThread::EThreadState_EventWantQuit)
								{
									{
										auto Checkout = fg_GetSys()->f_MemoryManager_Checkout();
										while (f_DispatchOne())
											;
									}

									m_Semaphore.f_Wait();
								}

								return 0;
							}
							, NStr::fg_Format("Thread pool numa {} thread {}", int32(*iNumaNode), nThreads)
							, NMib::EThreadPriority_Normal
							, 4*1024*1024
						)
					)
				;
			}
		}
	}

	CThreadPool::~CThreadPool()
	{
		for (auto &Thread : m_Threads)
		{
			Thread->f_Stop(false);
		}

		for (auto &Thread : m_Threads)
		{
			while (Thread->f_GetState() != NThread::EThreadState_Stopped)
				m_Semaphore.f_Signal();
		}

		m_Threads.f_Clear();
	}


	void CThreadPool::f_Dispatch(NFunction::TCFunction<void ()> && _Functor)
	{
//			DMibConOut("Queueing\n", 0);
		m_DispatchQueue.f_Push(fg_Move(_Functor));
		m_Semaphore.f_Signal();
	}
	void CThreadPool::f_Dispatch(NFunction::TCFunction<void ()> const& _Functor)
	{
//			DMibConOut("Queueing\n", 0);
		m_DispatchQueue.f_Push(_Functor);
		m_Semaphore.f_Signal();
	}

	bool CThreadPool::f_DispatchOne()
	{
		if (auto ToDispatch = m_DispatchQueue.f_Pop())
		{
			(*ToDispatch)();
			return true;
		}
		return false;
	}

	NStorage::TCAggregate<CThreadPool> g_ThreadPool = {DAggregateInit};

	CThreadPool &fg_DefaultThreadPool()
	{
		return *g_ThreadPool;
	}
}
