// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Private_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CActorDistributionManager::fp_CleanupRemoteContext(NFunction::TCFunction<void (CActorDistributionManagerInternal &_Internal)> const &_fCleanup)
	{
		_fCleanup(*mp_pInternal);
	}

	NPrivate::CDistributedActorStreamContextSendState::~CDistributedActorStreamContextSendState()
	{
		if (!m_pOwning)
			return;

		auto DistributionManager = m_pActorData->m_DistributionManager.f_Lock();
		if (!DistributionManager)
			return;

		DistributionManager
			(
				&CActorDistributionManager::fp_CleanupRemoteContext
				, [pOwning = fg_Move(m_pOwning), pActorData = m_pActorData, LastExecutionID = m_LastExecutionID](CActorDistributionManagerInternal &_Internal)
				{
					auto &DistributedData = *(static_cast<NActorDistributionManagerInternal::CDistributedActorDataInternal *>(pActorData.f_Get()));
					
					auto pHost = DistributedData.m_pHost.f_Lock();
					if (!pHost || pHost->m_LastExecutionID != LastExecutionID)
						return;
					
					for (auto &FunctionID : pOwning->m_Functions)
						pHost->m_ImplicitlyPublishedFunctions.f_Remove(FunctionID);
					for (auto &ActorID : pOwning->m_Actors)
						pHost->m_ImplicitlyPublishedActors.f_Remove(ActorID);
				}
			)
			> fg_DiscardResult()
		;
	}

	TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> CActorDistributionManager::f_CallRemote
		(
			NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
			, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> &&_CallData 
			, NPrivate::CDistributedActorStreamContextSending const &_Context
		)
	{
		auto &State = *_Context.m_pState; 
		State.m_pActorData = _pDistributedActorData;
		
		auto &DistributedData = *(static_cast<CActorDistributionManagerInternal::CDistributedActorDataInternal *>(_pDistributedActorData.f_Get()));

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
		
		State.m_LastExecutionID = pHost->m_LastExecutionID;
		
		if (!State.m_Actor.f_IsEmpty() || !State.m_Functions.f_IsEmpty())
		{
			if (State.m_Actor.f_IsEmpty())
				return DMibErrorInstance("You must have one and only one actor in the function arguments if you include a functor");
			if (State.m_Functions.f_IsEmpty())
				return DMibErrorInstance("You must have at least one function in arguments if you include an actor");
			
			auto &Owning = *(State.m_pOwning = fg_Construct());
			Owning.m_Actors[State.m_ActorID];
			pHost->m_ImplicitlyPublishedActors[State.m_ActorID] = State.m_Actor;
			
			for (auto &pFunction : State.m_Functions)
			{
				auto &FunctionID = State.m_Functions.fs_GetKey(pFunction);
				Owning.m_Functions[FunctionID];
				auto &Function = pHost->m_ImplicitlyPublishedFunctions[FunctionID];
				Function.m_pFunction = fg_Move(pFunction);
				Function.m_AllowedDisptachActorID = State.m_ActorID;
			}
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
	
	bool CActorDistributionManagerInternal::fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
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
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, NException::CExceptionBase const &_Exception)
	{
		auto Data = NPrivate::fg_StreamAsyncResultException(_Exception);
		
		CDistributedActorCommand_RemoteCallResult Result;
		Result.m_ReplyToPacketID = _PacketID;
		
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
		Stream << Result;
		Stream.f_FeedBytes(Data.f_GetArray(), Data.f_GetLen());
		
		fp_QueuePacket(_pHost, Stream.f_MoveVector());
	}
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCallWithException(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, uint64 _PacketID, CAsyncResult const &_Exception)
	{
		auto Data = NPrivate::fg_StreamAsyncResultException(_Exception);
		
		CDistributedActorCommand_RemoteCallResult Result;
		Result.m_ReplyToPacketID = _PacketID;
		
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
		Stream << Result;
		Stream.f_FeedBytes(Data.f_GetArray(), Data.f_GetLen());
		
		fp_QueuePacket(_pHost, Stream.f_MoveVector());
	}
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCall
		(
			NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost
			, uint64 _PacketID
			, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data
			, NPrivate::CDistributedActorStreamContextReceiving const &_Context
		)
	{
		auto &Host = *_pHost;
		if (Host.m_bDeleted)
			return;
		
		CDistributedActorCommand_RemoteCallResult Result;
		Result.m_ReplyToPacketID = _PacketID;
		
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
		Stream << Result;
		Stream.f_FeedBytes(_Data.f_GetArray(), _Data.f_GetLen());
		
		auto &State = *_Context.m_pState;
		
		if (!State.m_Subscription.f_IsEmpty())
		{
			auto &PublishedSubscription = Host.m_ImplicitlyPublishedSubscriptions[State.m_SubscriptionID];
			PublishedSubscription.m_Subscription = fg_Move(State.m_Subscription);
			PublishedSubscription.m_ReferencedActors = fg_Move(State.m_ActorReferences); 
		}
		
		fp_QueuePacket(_pHost, Stream.f_MoveVector());
	}
	
	CActorSubscription CActorDistributionManager::fp_OnRemoteDisconnect
		(
			TCActor<CActor> const &_Actor
			, NFunction::TCFunctionMutable<void ()> &&_fOnDisconnect
			, NStr::CStr const &_UniqueHostID
			, NStr::CStr const &_LastExecutionID
		)
	{
		auto &Internal = *mp_pInternal;
		
		auto *pHost = Internal.m_Hosts.f_FindEqual(_UniqueHostID);
		
		if (!pHost || (*pHost)->m_LastExecutionID != _LastExecutionID)
		{
			// Report disconnect
			fg_Dispatch(_Actor, fg_Move(_fOnDisconnect)) > fg_DiscardResult();
			return nullptr;
		}
		
		auto &Host = **pHost;
		
		return Host.m_OnDisconnect.f_Register(_Actor, fg_Move(_fOnDisconnect));			
	}
	
	bool CActorDistributionManagerInternal::fp_ApplyRemoteCall(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		CDistributedActorCommand_RemoteCall RemoteCall;
		_Stream >> RemoteCall;
		
		uint32 FunctionHash;
		_Stream >> FunctionHash;
		uint32 ProtocolVersion;
		_Stream >> ProtocolVersion;

		auto &pHost = _pConnection->m_pHost;
		
		NFunction::TCFunctionMovable<NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> (NStream::CBinaryStreamMemoryPtr<> &_Stream)> fCall;
		
		TCActor<> Actor;
		if (FunctionHash == 0)
		{
			NStr::CStr FunctionID;
			_Stream >> FunctionID; // This is a general functor

			auto *pImplicitFunction = pHost->m_ImplicitlyPublishedFunctions.f_FindEqual(FunctionID);
			
			if (!pImplicitFunction)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Subscription has been removed"));
				return true;
			}
			
			if (pImplicitFunction->m_AllowedDisptachActorID != RemoteCall.m_ActorID)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Trying to call function on wrong dispatch actor"));
				return true;
			}
			
			auto *pPublishedActor = pHost->m_ImplicitlyPublishedActors.f_FindEqual(RemoteCall.m_ActorID);
			
			if (pPublishedActor)
				Actor = pPublishedActor->f_Lock();
			
			if (!Actor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote dispatch actor no longer exists"));
				return true;
			}
			
			fCall = [pFunction = pImplicitFunction->m_pFunction](NStream::CBinaryStreamMemoryPtr<> &_Stream) mutable
				{
					return pFunction->f_Call(_Stream);					
				}
			;
		}
		else
		{
			auto pPublishedActor = m_PublishedActors.f_FindEqual(RemoteCall.m_ActorID);
			
			if (!pPublishedActor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"));
				return true;
			}
			
			if (pHost->m_bAnonymous && !fp_NamespaceAllowedForAnonymous(pPublishedActor->m_pNamespace->f_GetNamespace()))
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Access denied"));
				return true;
			}
			
			Actor = pPublishedActor->m_Actor.f_Lock();
			if (!Actor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"));
				return true;
			}
			
			auto &TypeRegistry = fg_RuntimeTypeRegistry();
			
			auto pEntry = TypeRegistry.m_EntryByHash_MemberFunction.f_FindEqual(FunctionHash);
			
			if (!pEntry || pPublishedActor->m_Hierarchy.f_BinarySearch(pEntry->m_TypeHash) < 0)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Function does not exist on remote actor"));
				return true;
			}
			
			fCall = [pEntry, pActor = Actor->fp_GetActor()](NStream::CBinaryStreamMemoryPtr<> &_Stream) mutable
				{
					return pEntry->f_Call
						(
							_Stream
							, pActor
						)
					;					
				}
			;
		}
		
		NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> ParamData;
		{
			mint nBytes = _Stream.f_GetLength() - _Stream.f_GetPosition();
			ParamData.f_SetLen(nBytes);
			_Stream.f_ConsumeBytes(ParamData.f_GetArray(), nBytes);
		}

		NPrivate::CDistributedActorStreamContextReceiving Context;
		auto &ContextState = *Context.m_pState;
		ContextState.m_pHost = pHost;
		ContextState.m_DistributionManager = fg_ThisActor(m_pThis);

		Actor
			(
				&CActor::f_DispatchWithReturn<NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>>
				, 
				[
					Actor
					, ParamData = fg_Move(ParamData)
					, HostID = pHost->m_RealHostID
					, CallingHostInfo = CCallingHostInfo(fg_ThisActor(m_pThis), pHost->m_UniqueHostID, pHost->f_GetHostInfo(), pHost->m_LastExecutionID, ProtocolVersion)
					, Context
					, fCall = fg_Move(fCall)
					, ProtocolVersion
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
					DMibBinaryStreamContext(Stream, &Context);
					DMibBinaryStreamVersion(Stream, ProtocolVersion);
					return fCall(Stream);
				}
			) 
			> 
			[
				this
				, pHost
				, PacketID = RemoteCall.m_PacketID
				, Context
				, LastExecutionID = pHost->m_LastExecutionID
			]
			(TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result) mutable
			{
				if (LastExecutionID != pHost->m_LastExecutionID)
					return;
				if (!_Result)
				{
					fp_ReplyToRemoteCallWithException(pHost, PacketID, _Result);
					return;
				}
				
				fp_ReplyToRemoteCall(pHost, PacketID, *_Result, Context);
			}
		;
		return true;
	}

	void CActorDistributionManager::fp_RegisterRemoteSubscription
		(
			NPrivate::CDistributedActorStreamContextSending const &_Context
			, NStr::CStr const &_SubscriptionID 
		)
	{
		auto &State = *_Context.m_pState;
		auto &DistributedData = *(static_cast<NActorDistributionManagerInternal::CDistributedActorDataInternal *>(State.m_pActorData.f_Get()));
		auto pHost = DistributedData.m_pHost.f_Lock();
		if (!pHost)
			return;
		auto &Host = *pHost;
		if (Host.m_bDeleted || Host.m_LastExecutionID != _Context.m_pState->m_LastExecutionID)
			return;
		
		auto &References = Host.m_SubscriptionReferences[_SubscriptionID];
		
		auto pOwning = fg_Move(State.m_pOwning);
		DMibCheck(pOwning);
		if (!pOwning)
			return;
		References.m_Actors = fg_Move(pOwning->m_Actors);
		References.m_Functions = fg_Move(pOwning->m_Functions);
	}
	
	void CActorDistributionManager::fp_DestroyRemoteSubscription
		(
			NPtr::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
			, NStr::CStr const &_SubscriptionID
			, NStr::CStr const &_LastExecutionID
		)
	{
		auto &DistributedData = *(static_cast<NActorDistributionManagerInternal::CDistributedActorDataInternal *>(_pDistributedActorData.f_Get()));
		auto pHost = DistributedData.m_pHost.f_Lock();
		if (!pHost || pHost->m_bDeleted || pHost->m_LastExecutionID != _LastExecutionID)
			return;
		auto &Host = *pHost;
		auto *pSubscription = Host.m_SubscriptionReferences.f_FindEqual(_SubscriptionID);
		DMibCheck(pSubscription);
		if (!pSubscription)
			return;
		
		for (auto &ActorID : pSubscription->m_Actors)
			Host.m_ImplicitlyPublishedActors.f_Remove(ActorID);
		for (auto &FunctionID : pSubscription->m_Functions)
			Host.m_ImplicitlyPublishedFunctions.f_Remove(FunctionID);

		CDistributedActorCommand_DestroySubscription Result;
		Result.m_SubscriptionID = _SubscriptionID;
		
		auto &Internal = *mp_pInternal;
		
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
		Stream << Result;
		Internal.fp_QueuePacket(pHost, Stream.f_MoveVector());
	}
	
	bool CActorDistributionManagerInternal::fp_HandleDestroySubscription(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		if (pHost->m_bDeleted)
			return true;

		CDistributedActorCommand_DestroySubscription DestroySubscription;
		_Stream >> DestroySubscription;
		pHost->m_ImplicitlyPublishedSubscriptions.f_Remove(DestroySubscription.m_SubscriptionID);
		
		return true;
	}
}

namespace NMib::NConcurrency::NPrivate
{
	CDistributedActorStreamContextReceiving::CDistributedActorStreamContextReceiving()
		: m_pState(fg_Construct())
	{
	}
	
	CDistributedActorStreamContextReceiving::~CDistributedActorStreamContextReceiving()
	{
	}

	CDistributedActorStreamContextReceiving::CDistributedActorStreamContextReceiving(CDistributedActorStreamContextReceiving const &) = default;
	CDistributedActorStreamContextReceiving::CDistributedActorStreamContextReceiving(CDistributedActorStreamContextReceiving &&) = default;
	CDistributedActorStreamContextReceiving& CDistributedActorStreamContextReceiving::operator =(CDistributedActorStreamContextReceiving const &) = default;
	CDistributedActorStreamContextReceiving& CDistributedActorStreamContextReceiving::operator =(CDistributedActorStreamContextReceiving &&) = default;

	CDistributedActorStreamContextSending::CDistributedActorStreamContextSending(NPtr::TCSharedPointer<CDistributedActorStreamContextSendState> &&_pState)
		: m_pState(fg_Move(_pState))
	{
	}
	
	CDistributedActorStreamContextSending::CDistributedActorStreamContextSending()
		: m_pState(fg_Construct())
	{
	}
	
	CDistributedActorStreamContextSending::~CDistributedActorStreamContextSending()
	{
	}
	
	bool CDistributedActorStreamContextSending::f_ValueReceived()
	{
		m_pState->m_bReceivedValue = true;
		return m_pState->f_IsValid();
	}

	CDistributedActorStreamContextSending::CDistributedActorStreamContextSending(CDistributedActorStreamContextSending const &) = default;
	CDistributedActorStreamContextSending::CDistributedActorStreamContextSending(CDistributedActorStreamContextSending &&) = default;
	CDistributedActorStreamContextSending& CDistributedActorStreamContextSending::operator =(CDistributedActorStreamContextSending const &) = default;
	CDistributedActorStreamContextSending& CDistributedActorStreamContextSending::operator =(CDistributedActorStreamContextSending &&) = default;
	
	CStreamingFunction::~CStreamingFunction()
	{
	}

	bool CDistributedActorStreamContextSendState::f_IsValid() const
	{
		if (!m_bReceivedValue)
			return true;
		
		if (m_Functions.f_IsEmpty() && m_Actor.f_IsEmpty())
			return true;

		if (m_Actor.f_IsEmpty())
			return false;
		
		if (!m_bSubscriptionReturned)
			return false;
		
		return true; 
	}
	
	CStreamSubscriptionSend::CStreamSubscriptionSend(CActorSubscription &_Subscription, CDistributedActorStreamContextSendState &_State)
		: m_Subscription(_Subscription)
		, m_State(_State)
	{
	}
	
	CStreamSubscriptionSend::CSubscriptionReference::~CSubscriptionReference()
	{
		auto DistributionManager = mp_pActorData->m_DistributionManager.f_Lock();
		if (!DistributionManager)
			return;
		DistributionManager(&CActorDistributionManager::fp_DestroyRemoteSubscription, mp_pActorData, mp_SubscriptionID, mp_LastExecutionID) > fg_DiscardResult();
	}

	void CStreamSubscriptionSend::f_Consume(CDistributedActorReadStream &_Stream)
	{
		NStr::CStr SubscriptionID;
		_Stream >> SubscriptionID;

		NPtr::TCUniquePointer<CSubscriptionReference> pSubscription = fg_Construct();
		pSubscription->mp_pActorData = m_State.m_pActorData;
		pSubscription->mp_SubscriptionID = SubscriptionID;
		pSubscription->mp_LastExecutionID = m_State.m_LastExecutionID;
		
		auto DistributionManager = m_State.m_pActorData->m_DistributionManager.f_Lock();
		DMibCheck(DistributionManager); // Should be called from place whene distribution manager is referenced
		if (DistributionManager)
		{
			DistributionManager
				(
					&CActorDistributionManager::fp_RegisterRemoteSubscription
					, NPrivate::CDistributedActorStreamContextSending{fg_Explicit(&m_State)}
					, SubscriptionID
				)
				> fg_DiscardResult()
			;
		}
		
		m_Subscription = fg_Move(pSubscription);

		m_State.m_bSubscriptionReturned = true;
	}
	
	CStreamActorSend::CStreamActorSend(TCActor<CActor> const &_Actor, CDistributedActorStreamContextSendState &_State)
		: m_Actor(_Actor)
		, m_State(_State)
	{
	}
	
	void CStreamActorSend::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		DMibCheck(m_State.m_ActorID.f_IsEmpty()); // You can send one and only one actor in the call, otherwise it's impossible to know which actor to dispatch the functions to 
		NStr::CStr ActorID = NCryptography::fg_RandomID();
		_Stream << ActorID;
		m_State.m_ActorID = ActorID;
		m_State.m_Actor = m_Actor;
	}
	
	CStreamFunctionSend::CStreamFunctionSend(NPtr::TCSharedPointer<CStreamingFunction> &&_pFunction, CDistributedActorStreamContextSendState &_State)
		: m_pFunction(fg_Move(_pFunction))
		, m_State(_State) 
	{
	}
	
	void CStreamFunctionSend::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		NStr::CStr FunctionID = NCryptography::fg_RandomID();
		_Stream << FunctionID;
		m_State.m_Functions[FunctionID] = fg_Move(m_pFunction);
	}
	
	CStreamActorReceive::CStreamActorReceive(TCActor<CActor> &_Actor, CDistributedActorStreamContextReceiveState &_State)
		: m_Actor(_Actor)
		, m_State(_State)
	{
	}
	
	class CRemoteDispatchActorActorHolder : public CDefaultActorHolder
	{
	public:
		CRemoteDispatchActorActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
			: CDefaultActorHolder(_pConcurrencyManager, _bImmediateDelete, _Priority, fg_Move(_pDistributedActorData))
		{
		}
		
		void f_QueueProcess(FActorQueueDispatch &&_Functor, bool _bSame = false) override
		{
			CDefaultActorHolder::f_QueueProcess
				(
					[this, Functor = fg_Move(_Functor)]() mutable
					{
						// this is referenced by shared pointer in CDefaultActorHolder::f_QueueProcess
						
						auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;
						auto pOldDispatchData = ThreadLocal.m_pCurrentRemoteDispatchActorData;
						ThreadLocal.m_pCurrentRemoteDispatchActorData = static_cast<CDistributedActorData *>(mp_pDistributedActorData.f_Get());
						auto Cleanup = g_OnScopeExit > [&]
							{
								ThreadLocal.m_pCurrentRemoteDispatchActorData = pOldDispatchData; 
							}
						;
						Functor();
					}
					, _bSame 
				)
			;
		}
	};
	
	struct CRemoteDispatchActor : public CActor
	{
		using CActorHolder = CRemoteDispatchActorActorHolder;
	};

	void CStreamActorReceive::f_Consume(CDistributedActorReadStream &_Stream)
	{
		using namespace NActorDistributionManagerInternal;
		NStr::CStr ActorID;
		_Stream >> ActorID;
		
		auto DistributionManager = m_State.m_DistributionManager.f_Lock();
		
		NPtr::TCSharedPointer<CDistributedActorDataInternal> pDistributedActorData = fg_Construct();
		pDistributedActorData->m_pHost = m_State.m_pHost;
		pDistributedActorData->m_bRemote = true;
		pDistributedActorData->m_DistributionManager = m_State.m_DistributionManager;
		pDistributedActorData->m_ActorID = ActorID;
		pDistributedActorData->m_ProtocolVersion = _Stream.f_GetVersion();
		DMibFastCheck(pDistributedActorData->m_ProtocolVersion != TCLimitsInt<uint32>::mc_Max);
		
		auto &ConcurrencyManager = DistributionManager->f_ConcurrencyManager(); 
		
		NPtr::TCSharedPointer<TCActorInternal<TCDistributedActorWrapper<CRemoteDispatchActor>>, NPtr::CSupportWeakTag, CInternalActorAllocator> pActor 
			= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData))
		;
		
		m_Actor = ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<CRemoteDispatchActor>>
			(
				fg_Move(pActor)
				, fg_Construct<TCDistributedActorWrapper<CRemoteDispatchActor>>()
			)
		;
		
		m_State.m_ActorReferences.f_Insert(m_Actor);
	}
	
	CStreamSubscriptionReceive::CStreamSubscriptionReceive(CActorSubscription &&_Subscription, CDistributedActorStreamContextReceiveState &_State)
		: m_Subscription(fg_Move(_Subscription))
		, m_State(_State)
	{
	}
	
	void CStreamSubscriptionReceive::f_Feed(CDistributedActorWriteStream &_Stream) const
	{
		NStr::CStr SubscriptionID = NCryptography::fg_RandomID();
		_Stream << SubscriptionID;
		DMibCheck(m_State.m_Subscription.f_IsEmpty());
		m_State.m_SubscriptionID = SubscriptionID;
		m_State.m_Subscription = fg_Move(m_Subscription);
	}
}
