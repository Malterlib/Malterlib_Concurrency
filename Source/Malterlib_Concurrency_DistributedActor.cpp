// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			CSubSystem_Concurrency_DistributedActor::CSubSystem_Concurrency_DistributedActor()
			{
				fg_ConcurrencyManager(); // Add dependency to subsystem
				
				m_TestActor = fg_ConstructActor<CActor>();
			}
			
			CSubSystem_Concurrency_DistributedActor::~CSubSystem_Concurrency_DistributedActor()
			{
			}
			
			void CSubSystem_Concurrency_DistributedActor::f_DestroyThreadSpecific()
			{
				m_TestActor.f_Clear();
			}			
			
			TCSubSystem<CSubSystem_Concurrency_DistributedActor, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_DistributedActor = {DAggregateInit};
			
			CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem()
			{
				return *g_MalterlibSubSystem_Concurrency_DistributedActor;
			}
			
			template <>
			void fg_CopyReplyToContinuation(TCContinuation<void> &_Continuation, NContainer::TCVector<uint8> const &_Data)
			{
				NStream::CBinaryStreamMemoryPtr<> ReplyStream;
				ReplyStream.f_OpenRead(_Data);
				if (fg_CopyReplyToContinuationShared(ReplyStream, _Continuation))
					return;
				
				_Continuation.f_SetResult();
			}
		}			
	}
}
