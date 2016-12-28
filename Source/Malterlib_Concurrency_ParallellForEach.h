// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ThreadSafeQueue>
#include <Mib/Concurrency/AsyncResult>

namespace NMib
{
	namespace NConcurrency
	{
		struct CThreadPool
		{
			CThreadPool(mint _nThreads = fg_Max(aint(NSys::fg_Thread_GetVirtualCores()) - 3, 0));
			~CThreadPool();
			
			void f_Dispatch(NFunction::TCFunction<void ()> && _Functor);
			void f_Dispatch(NFunction::TCFunction<void ()> const& _Functor);
			bool f_DispatchOne();
		
		private:
			NContainer::TCVector<NPtr::TCUniquePointer<NThread::CThreadObject>> m_Threads;
			NContainer::TCThreadSafeQueue<NFunction::TCFunction<void ()>> m_DispatchQueue;
			
			NThread::CSemaphore m_Semaphore;
		};
		
		extern NAggregate::TCAggregate<CThreadPool> g_ThreadPool;
		
		template <typename tf_CContainer, typename tf_CFunctor>
		void fg_ParallellForEach(tf_CContainer &&_Container, tf_CFunctor &&_Functor);
	}
}

#include "Malterlib_Concurrency_ParallellForEach.hpp"

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
