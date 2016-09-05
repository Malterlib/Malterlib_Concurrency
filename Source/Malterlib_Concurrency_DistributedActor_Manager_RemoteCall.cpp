// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib
{
	namespace NConcurrency
	{
		TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> CActorDistributionManager::f_CallRemote
			(
				NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
				, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_CallData 
			)
		{
			auto &DistributedData = *(static_cast<CInternal::CDistributedActorDataInternal *>(_pDistributedActorData.f_Get()));

			TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Continuation;

			if (DistributedData.m_bWasDestroyed)
			{
				Continuation.f_SetException(DMibErrorInstance("Remote actor longer available"));
				return Continuation;
			}
			
			auto pHost = DistributedData.m_pHost.f_Lock();
			
			if (!pHost || pHost->m_bDeleted)
			{
				Continuation.f_SetException(DMibErrorInstance("Remote actor host no longer available"));
				return Continuation;
			}
			
			{
				NStream::CBinaryStreamMemoryPtr<> Stream;
				Stream.f_OpenReadWrite(_CallData.f_GetArray(), _CallData.f_GetLen(), _CallData.f_GetLen());
				Stream << uint8(EDistributedActorCommand_RemoteCall);
			}
			
			auto &Internal = *mp_pInternal;
			
			auto PacketID = Internal.fp_QueuePacket(pHost, fg_Move(_CallData));
			pHost->m_OutstandingCalls[PacketID] = Continuation;

			return Continuation;			
		}
		
		bool CActorDistributionManager::CInternal::fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
		{
			CDistributedActorCommand_RemoteCallResult RemoteCallResult;
			_Stream >> RemoteCallResult;
			
			auto &Host = *_pConnection->m_pHost; 
			auto pCall = Host.m_OutstandingCalls.f_FindEqual(RemoteCallResult.m_ReplyToPacketID);
			if (!pCall)
				return false;
			
			mint nBytes = _Stream.f_GetLength() - _Stream.f_GetPosition();
			
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> Data;
			Data.f_SetLen(nBytes);
			_Stream.f_ConsumeBytes(Data.f_GetArray(), nBytes);
			pCall->f_SetResult(fg_Move(Data));
			
			Host.m_OutstandingCalls.f_Remove(pCall);
			return true;
		}
		
		void CActorDistributionManager::CInternal::fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, NException::CExceptionBase const &_Exception)
		{
			auto Data = NPrivate::fg_StreamAsyncResultException(_Exception);
			
			CDistributedActorCommand_RemoteCallResult Result;
			Result.m_ReplyToPacketID = _PacketID;
			
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Result;
			Stream.f_FeedBytes(Data.f_GetArray(), Data.f_GetLen());
			
			fp_QueuePacket(_pHost, Stream.f_MoveVector());
		}
		
		void CActorDistributionManager::CInternal::fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, CAsyncResult const &_Exception)
		{
			auto Data = NPrivate::fg_StreamAsyncResultException(_Exception);
			
			CDistributedActorCommand_RemoteCallResult Result;
			Result.m_ReplyToPacketID = _PacketID;
			
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Result;
			Stream.f_FeedBytes(Data.f_GetArray(), Data.f_GetLen());
			
			fp_QueuePacket(_pHost, Stream.f_MoveVector());
		}
		
		void CActorDistributionManager::CInternal::fp_ReplyToRemoteCall(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data)
		{
			CDistributedActorCommand_RemoteCallResult Result;
			Result.m_ReplyToPacketID = _PacketID;
			
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Result;
			Stream.f_FeedBytes(_Data.f_GetArray(), _Data.f_GetLen());
			
			fp_QueuePacket(_pHost, Stream.f_MoveVector());
		}
		
		CActorSubscription CActorDistributionManager::fp_OnRemoteDisconnect
			(
				TCActor<CActor> const &_Actor
				, NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fOnDisconnect
				, NStr::CStr const &_UniqueHostID
				, NStr::CStr const &_LastExecutionID
			)
		{
			auto &Internal = *mp_pInternal;
			
			auto *pHost = Internal.m_Hosts.f_FindEqual(_UniqueHostID);
			
			if (!pHost || (*pHost)->m_LastExecutionID != _LastExecutionID)
			{
				// Report disconnect
				fg_Dispatch(_Actor, _fOnDisconnect) > fg_DiscardResult();
				return nullptr;
			}
			
			auto &Host = **pHost;
			
			return Host.m_OnDisconnect.f_Register(_Actor, fg_Move(_fOnDisconnect));			
		}
		
		bool CActorDistributionManager::CInternal::fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
		{
			CDistributedActorCommand_RemoteCall RemoteCall;
			_Stream >> RemoteCall;
			
			uint32 FunctionHash;
			_Stream >> FunctionHash;
			
			auto &pHost = _pConnection->m_pHost;
			
			auto pActor = m_PublishedActors.f_FindEqual(RemoteCall.m_ActorID);
			
			if (!pActor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"));
				return true;
			}
			
			if (pHost->m_bAnonymous && !fp_NamespaceAllowedForAnonymous(pActor->m_pNamespace->f_GetNamespace()))
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Access denied"));
				return true;
			}
			
			auto Actor = pActor->m_Actor.f_Lock();
			if (!Actor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"));
				return true;
			}
			
			auto &TypeRegistry = fg_RuntimeTypeRegistry();
			
			auto pEntry = TypeRegistry.m_EntryByHash_MemberFunction.f_FindEqual(FunctionHash);
			
			if (!pEntry || pActor->m_Hierarchy.f_BinarySearch(pEntry->m_TypeHash) < 0)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Function does not exist on remote actor"));
				return true;
			}
			
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> ParamData;
			{
				mint nBytes = _Stream.f_GetLength() - _Stream.f_GetPosition();
				ParamData.f_SetLen(nBytes);
				_Stream.f_ConsumeBytes(ParamData.f_GetArray(), nBytes);
			}
			
			Actor
				(
					&CActor::f_DispatchWithReturn<NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>>
					, 
					[
						pEntry
						, Actor
						, ParamData = fg_Move(ParamData)
						, HostID = pHost->m_RealHostID
						, CallingHostInfo = CCallingHostInfo(fg_ThisActor(m_pThis), pHost->m_UniqueHostID, pHost->f_GetHostInfo(), pHost->m_LastExecutionID)
					] 
					() mutable
					{
						auto &ThreadLocal = *NPrivate::fg_DistributedActorSubSystem().m_ThreadLocal;
						auto PreviousHostInfo = fg_Move(ThreadLocal.m_CallingHostInfo);
						ThreadLocal.m_CallingHostInfo = fg_Move(CallingHostInfo);
						auto Cleanup = g_OnScopeExit > [&]
							{
								ThreadLocal.m_CallingHostInfo = fg_Move(PreviousHostInfo);
							}
						;
						
						NStream::CBinaryStreamMemoryPtr<> Stream;
						Stream.f_OpenRead(ParamData);
						return pEntry->f_Call
							(
								Stream
								, &Actor->f_AccessInternal()
							)
						;
					}
				) 
				> [this, pHost, PacketID = RemoteCall.m_PacketID](TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result)
				{
					if (!_Result)
					{
						fp_ReplyToRemoteCallWithException(pHost, PacketID, _Result);
						return;
					}
					
					fp_ReplyToRemoteCall(pHost, PacketID, *_Result);
				}
			;
			return true;
		}		
	}
}
