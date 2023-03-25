// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Stream_Internal.h"
#include "Malterlib_Concurrency_DistributedActor_Manager_Protocol.h"

namespace NMib::NConcurrency
{
	void CActorDistributionManager::fp_CleanupRemoteContext(NFunction::TCFunction<void (CActorDistributionManagerInternal &_Internal)> const &_fCleanup)
	{
		_fCleanup(*mp_pInternal);
	}

	NPrivate::CDistributedActorStreamContextState::CDistributedActorStreamContextState(uint32 _ActorProtocolVersion, bool _bCallInitiator)
		: m_ActorProtocolVersion(_ActorProtocolVersion)
		, m_bCallInitiator(_bCallInitiator)
	{
	}

	inline_never static void fg_DestroyStreamedFunctionOnActor(TCActor<CActor> const &_Actor, NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &o_pFunction)
	{
		g_Dispatch(_Actor) / [pFunction = fg_Move(o_pFunction)]
			{
			}
			> fg_DiscardResult()
		;
	}

	inline_never static void fg_DestroyStreamedFunctionWithoutActor(NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &o_pFunction)
	{
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		mint DestroySequence = ThreadLocal.m_AllowWrongThreadDestroySequence;

		CMibCodeAddress volatile pFunctionPointer = (CMibCodeAddress)o_pFunction->f_GetFunctionPointer();
		(void)pFunctionPointer;
			
		o_pFunction.f_Clear();
		
		if (DestroySequence == ThreadLocal.m_AllowWrongThreadDestroySequence)
			DMibPDebugBreak; 
		/*
		 Destroying actor functor on wrong thread
		 To allow destroy on wrong thread, capture allow destroy in the lambda: 
		 
			/ [AllowDestroy = g_AllowWrongThreadDestroy]
			{
			}
		 ;
		 */
#else
		o_pFunction.f_Clear();
#endif
	}

	static void fg_DestroyStreamedFunction(TCWeakActor<CActor> const &_Actor, NStorage::TCSharedPointer<NPrivate::CStreamingFunction> &o_pFunction)
	{
		auto Actor = _Actor.f_Lock();
		if (Actor)
			return fg_DestroyStreamedFunctionOnActor(Actor, o_pFunction);

		fg_DestroyStreamedFunctionWithoutActor(o_pFunction);
	}
	
	NPrivate::CDistributedActorStreamContextState::CActorFunctorInfo::CActorFunctors::CActorFunctors() = default;

	NPrivate::CDistributedActorStreamContextState::CActorFunctorInfo::CActorFunctors::~CActorFunctors()
	{
		for (auto &pFunction : m_Functions)
		{
			if (!pFunction || pFunction->f_IsEmpty())
				continue;
			
			fg_DestroyStreamedFunction(m_Actor, pFunction);
		}
	}
	
	void NActorDistributionManagerInternal::CHost::f_DestroyImplicitFunction(NStr::CStr const &_FunctionID)
	{
		auto *pFunction = m_ImplicitlyPublishedFunctions.f_FindEqual(_FunctionID);
		if (!pFunction)
			return;
		
		if (pFunction->m_pFunction && !pFunction->m_pFunction->f_IsEmpty())
		{
			auto *pActor = m_ImplicitlyPublishedActors.f_FindEqual(pFunction->m_AllowedDisptachActorID);
			if (pActor)
				fg_DestroyStreamedFunction(*pActor, pFunction->m_pFunction);
			else
				fg_DestroyStreamedFunction({}, pFunction->m_pFunction);
		}

		m_ImplicitlyPublishedFunctions.f_Remove(pFunction);
	}

	NPrivate::CDistributedActorStreamContextState::~CDistributedActorStreamContextState()
	{
		NContainer::TCSet<NStr::CStr> ImplicitlyPublishedFunctions;
		NContainer::TCSet<NStr::CStr> ImplicitlyPublishedActors;
		NContainer::TCSet<NStr::CStr> ImplicitlyPublishedInterfaces;
		
		for (auto &ActorFuntor : m_ActorFunctors)
		{
			ImplicitlyPublishedActors += ActorFuntor.m_ImplicitlyPublishedActors;
			ImplicitlyPublishedFunctions += ActorFuntor.m_ImplicitlyPublishedFunctions;
			ImplicitlyPublishedInterfaces += ActorFuntor.m_ImplicitlyPublishedInterfaces;
		}
		
		if (ImplicitlyPublishedFunctions.f_IsEmpty() && ImplicitlyPublishedActors.f_IsEmpty() && ImplicitlyPublishedInterfaces.f_IsEmpty())
			return;
		
		auto DistributionManager = m_DistributionManager.f_Lock();
		if (!DistributionManager)
			return;

		DistributionManager
			(
				&CActorDistributionManager::fp_CleanupRemoteContext
				, 
				[
					ImplicitlyPublishedFunctions = fg_Move(ImplicitlyPublishedFunctions)
					, ImplicitlyPublishedActors = fg_Move(ImplicitlyPublishedActors)
					, ImplicitlyPublishedInterfaces = fg_Move(ImplicitlyPublishedInterfaces)
					, pHost = m_pHost
					, LastExecutionID = m_LastExecutionID
				]
				(CActorDistributionManagerInternal &_Internal)
				{
					if (pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || pHost->m_LastExecutionID != LastExecutionID)
						return;
					
					for (auto &FunctionID : ImplicitlyPublishedFunctions)
						pHost->f_DestroyImplicitFunction(FunctionID);
					for (auto &ActorID : ImplicitlyPublishedActors)
						pHost->m_ImplicitlyPublishedActors.f_Remove(ActorID);
					for (auto &ActorID : ImplicitlyPublishedInterfaces)
						pHost->m_ImplicitlyPublishedInterfaces.f_Remove(ActorID);
				}
			)
			> fg_DiscardResult()
		;
	}

	namespace
	{
		template <typename tf_CPromise>
		bool fg_RegisterActorFunctorsForCall(NPrivate::CDistributedActorStreamContextState &_State, NActorDistributionManagerInternal::CHost &_Host, tf_CPromise &_Promise)
		{
			for (auto &ActorFuntor : _State.m_ActorFunctors)
			{
				for (auto &Functor : ActorFuntor.m_ActorFunctors)
				{
					if (Functor.m_Actor.f_IsEmpty())
					{
						_Promise.f_SetException(DMibErrorInstance("You must have one and only one actor in the function arguments if you include a functor"));
						return false;
					}
					if (Functor.m_Functions.f_IsEmpty())
					{
						_Promise.f_SetException(DMibErrorInstance("You must have at least one function in arguments if you include an actor"));
						return false;
					}
					ActorFuntor.m_ImplicitlyPublishedActors[Functor.m_ActorID];
					_Host.m_ImplicitlyPublishedActors[Functor.m_ActorID] = Functor.m_Actor;
					
					for (auto &pFunction : Functor.m_Functions)
					{
						auto &FunctionID = Functor.m_Functions.fs_GetKey(pFunction);
						ActorFuntor.m_ImplicitlyPublishedFunctions[FunctionID];

						auto &Function = _Host.m_ImplicitlyPublishedFunctions[FunctionID];
						Function.m_pFunction = fg_Move(pFunction);
						Function.m_AllowedDisptachActorID = Functor.m_ActorID;
					}
				}
				for (auto &Interface : ActorFuntor.m_Interfaces)
				{
					if (Interface.m_Actor.f_IsEmpty())
					{
						_Promise.f_SetException(DMibErrorInstance("Invalid actor in interface"));
						return false;
					}
					ActorFuntor.m_ImplicitlyPublishedInterfaces[Interface.m_ActorID];
					auto &PublishedInterface = _Host.m_ImplicitlyPublishedInterfaces[Interface.m_ActorID];
					PublishedInterface.m_Actor = fg_Move(Interface.m_Actor);
					PublishedInterface.m_Hierarchy = fg_Move(Interface.m_Hierarchy);
					PublishedInterface.m_ProtocolVersions = Interface.m_ProtocolVersions;
				}
			}
			return true;
		}
	}
	
	bool CActorDistributionManagerInternal::fp_RegisterActorFunctorsForCall
		(
			NPrivate::CDistributedActorStreamContextState &_State
			, NActorDistributionManagerInternal::CHost &_Host
			, TCPromise<> &_Promise
		)
	{
		return fg_RegisterActorFunctorsForCall(_State, _Host, _Promise);
	}
	
	TCFuture<NContainer::CSecureByteVector> CActorDistributionManager::f_CallRemote
		(
			NStorage::TCSharedPointer<NPrivate::CDistributedActorData> const &_pDistributedActorData
			, NContainer::CSecureByteVector &&_CallData 
			, NPrivate::CDistributedActorStreamContext const &_Context
		)
	{
		TCPromise<NContainer::CSecureByteVector> Promise;

		if (f_IsDestroyed())
			return Promise <<= DMibErrorInstance("Distribution manager destroyed");

		auto &State = *_Context.m_pState; 
		auto &DistributedData = *_pDistributedActorData.f_Get();

		if (DistributedData.m_bWasDestroyed)
			return Promise <<= DMibErrorInstance("Remote actor longer available");

		auto pHostInterface = DistributedData.m_pHost.f_Lock();
		
		if (!pHostInterface || pHostInterface->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return Promise <<= DMibErrorInstance("Remote actor host no longer available");

		auto pHost = reinterpret_cast<NStorage::TCSharedPointerSupportWeak<NActorDistributionManagerInternal::CHost> &>(pHostInterface);

		if (State.m_ActorProtocolVersion != pHost->m_ActorProtocolVersion)
			return Promise <<= DMibErrorInstance("Remote host restarted with new actor protocol");

		auto &Internal = *mp_pInternal;

		if (_CallData.f_GetLen() > Internal.m_WebsocketSettings.m_MaxMessageSize)
			return Promise <<= DMibErrorInstance("Remote call size was larger than the max allowed packet size");

		State.m_pHost = pHost;
		State.m_DistributionManager = fg_ThisActor(this);
		State.m_LastExecutionID = pHost->m_LastExecutionID;
		
		if (!fg_RegisterActorFunctorsForCall(State, *pHost, Promise))
			return Promise.f_MoveFuture();
			
		{
			NStream::CBinaryStreamMemoryPtr<> Stream;
			Stream.f_OpenReadWrite(_CallData.f_GetArray(), _CallData.f_GetLen(), _CallData.f_GetLen());
			Stream << uint8(EDistributedActorCommand_RemoteCall);
		}
		

		auto PacketID = Internal.fp_QueuePacket(pHost, fg_Move(_CallData));
		auto &OutstandingCall = pHost->m_OutstandingCalls[PacketID];
		OutstandingCall.m_Promise = Promise;
		OutstandingCall.m_pState = _Context.m_pState;

		return Promise.f_MoveFuture();			
	}

	bool CActorDistributionManagerInternal::fp_ApplyRemoteCallResult(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		CDistributedActorCommand_RemoteCallResult RemoteCallResult;
		auto &Host = *_pConnection->m_pHost;
		uint32 ProtocolVersion = Host.m_ActorProtocolVersion.f_Load();
		DMibFastCheck(ProtocolVersion != 0);
		DMibBinaryStreamVersion(_Stream, ProtocolVersion);
		_Stream >> RemoteCallResult;
		
		auto pCall = Host.m_OutstandingCalls.f_FindEqual(RemoteCallResult.m_ReplyToPacketID);
		if (!pCall)
			return false;
		
		mint nBytes = _Stream.f_GetLength() - _Stream.f_GetPosition();
		
		NContainer::CSecureByteVector Data;
		if (nBytes != 0)
		{
			Data.f_SetLen(nBytes);
			_Stream.f_ConsumeBytes(Data.f_GetArray(), nBytes);
		}
		fp_RegisterImplicitSubscriptions(Host, *pCall->m_pState, &RemoteCallResult.m_SubscriptionData);
		pCall->m_Promise.f_SetResult(fg_Move(Data));

		Host.m_OutstandingCalls.f_Remove(pCall);
		return true;
	}
	
	void fg_TransferSubscriptionData(CResultSubscriptionData &o_SubscriptionData, NPrivate::CDistributedActorStreamContextState const &_State)
	{
		for (auto &ActorFuntor : _State.m_ActorFunctors)
		{
			for (auto &PendingSubscription : ActorFuntor.m_PendingSubscriptions)
				o_SubscriptionData.m_ClaimedSubscriptionIDs.f_Insert(PendingSubscription->m_SubscriptionID);
		}
	}
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCallWithException
		(
			NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
			, uint64 _PacketID
			, NException::CExceptionBase const &_Exception
			, NPrivate::CDistributedActorStreamContext const &_Context
		)
	{
		return fp_ReplyToRemoteCall(_pHost, _PacketID, NPrivate::fg_StreamAsyncResultException(_Exception, _Context.f_ActorProtocolVersion()), _Context);
	}
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCallWithException
		(
			NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
			, uint64 _PacketID
			, CAsyncResult const &_Exception
			, NPrivate::CDistributedActorStreamContext const &_Context
		)
	{
		return fp_ReplyToRemoteCall(_pHost, _PacketID, NPrivate::fg_StreamAsyncResultException(_Exception, _Context.f_ActorProtocolVersion()), _Context);
	}
	
	void CActorDistributionManagerInternal::fp_ReplyToRemoteCall
		(
			NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost
			, uint64 _PacketID
			, NContainer::CSecureByteVector const &_Data
			, NPrivate::CDistributedActorStreamContext const &_Context
		)
	{
		auto &State = *_Context.m_pState;
		auto &Host = *_pHost;
		if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || Host.m_LastExecutionID != State.m_LastExecutionID)
			return;

		CDistributedActorCommand_RemoteCallResult Result;
		Result.m_ReplyToPacketID = _PacketID;
		fg_TransferSubscriptionData(Result.m_SubscriptionData, State);
		
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		uint32 ProtocolVersion = Host.m_ActorProtocolVersion.f_Load();
		DMibFastCheck(ProtocolVersion != 0);
		DMibBinaryStreamVersion(Stream, ProtocolVersion);
		Stream << Result;
		mint DataLen = _Data.f_GetLen();
		if (DataLen != 0)
			Stream.f_FeedBytes(_Data.f_GetArray(), DataLen);

		auto SendPacket = Stream.f_MoveVector();

		if (SendPacket.f_GetLen() > m_WebsocketSettings.m_MaxMessageSize)
		{
			fp_ReplyToRemoteCallWithException(_pHost, _PacketID, DMibErrorInstance("Reply was larger than max allowed packet size"), _Context);
			return;
		}

		bool bIsException = _Data[0] != 0;
		if (!bIsException)
		{
			fp_RegisterLocalSubscriptions(State);
			fp_RegisterImplicitSubscriptions(Host, *_Context.m_pState, nullptr);
		}

		fp_QueuePacket(_pHost, fg_Move(SendPacket));
	}
	
	CActorSubscription CActorDistributionManager::fp_OnRemoteDisconnect
		(
			TCActorFunctorWeak<TCFuture<void> ()> &&_fOnDisconnect
			, NStr::CStr const &_UniqueHostID
			, NStr::CStr const &_LastExecutionID
		)
	{
		auto &Internal = *mp_pInternal;
		
		auto *pHost = Internal.m_Hosts.f_FindEqual(_UniqueHostID);
		
		if (!pHost || (*pHost)->m_LastExecutionID != _LastExecutionID)
		{
			// Report disconnect
			_fOnDisconnect() > fg_DiscardResult();
			return nullptr;
		}
		
		auto &Host = **pHost;

		auto &OnDisconnect = Host.m_OnDisconnect.f_Insert();
		OnDisconnect.m_fOnDisconnect = fg_Move(_fOnDisconnect);

		return g_ActorSubscription / [pHost = &Host, pOnDisconnect = &OnDisconnect, pDestroyed = OnDisconnect.m_pDestroyed]() -> TCFuture<void>
			{
				if (*pDestroyed)
					co_return {};

				auto ToDestroy = fg_Move(pOnDisconnect->m_fOnDisconnect);

				pHost->m_OnDisconnect.f_Remove(*pOnDisconnect);

				co_await fg_Move(ToDestroy).f_Destroy();

				co_return {};
			}
		;
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
		auto &Host = *pHost;

		NPrivate::CDistributedActorStreamContext Context{Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed), false};
		auto &ContextState = *Context.m_pState;
		ContextState.m_pHost = pHost;
		ContextState.m_DistributionManager = fg_ThisActor(m_pThis);
		ContextState.m_LastExecutionID = Host.m_LastExecutionID;
		
		NFunction::TCFunctionMovable<NConcurrency::TCFuture<NContainer::CSecureByteVector> (CDistributedActorReadStream &_Stream)> fCall;
		
		TCActor<> Actor;
		if (FunctionHash == 0)
		{
			NStr::CStr FunctionID;
			_Stream >> FunctionID; // This is a general functor

			auto *pImplicitFunction = Host.m_ImplicitlyPublishedFunctions.f_FindEqual(FunctionID);
			
			if (!pImplicitFunction)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Subscription has been removed"), Context);
				return true;
			}
			
			if (pImplicitFunction->m_AllowedDisptachActorID != RemoteCall.m_ActorID)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Trying to call function on wrong dispatch actor"), Context);
				return true;
			}
			
			auto *pPublishedActor = Host.m_ImplicitlyPublishedActors.f_FindEqual(RemoteCall.m_ActorID);
			
			if (pPublishedActor)
				Actor = pPublishedActor->f_Lock();
			
			if (!Actor)
			{
				fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote dispatch actor no longer exists"), Context);
				return true;
			}
			
			fCall = [pFunction = pImplicitFunction->m_pFunction](CDistributedActorReadStream &_Stream) mutable
				{
					return pFunction->f_Call(_Stream);
				}
			;
		}
		else
		{
			do
			{
				NActorDistributionManagerInternal::CPublishedInterface *pPublishedInterface;

				if (auto pPublishedActor = m_PublishedActors.f_FindEqual(RemoteCall.m_ActorID))
				{
					if (Host.m_HostInfo.m_bAnonymous && !fp_NamespaceAllowedForAnonymous(pPublishedActor->m_pNamespace->f_GetNamespace()))
					{
						fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Access denied"), Context);
						return true;
					}

					pPublishedInterface = pPublishedActor;
				}
				else
				{
					pPublishedInterface = Host.m_ImplicitlyPublishedInterfaces.f_FindEqual(RemoteCall.m_ActorID);
					if (!pPublishedInterface)
					{
						fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"), Context);
						return true;
					}
				}

				Actor = pPublishedInterface->m_Actor.f_Lock();
				if (!Actor)
				{
					fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Remote actor no longer exists"), Context);
					return true;
				}

				auto &TypeRegistry = fg_RuntimeTypeRegistry();
				auto pEntry = TypeRegistry.m_EntryByHash_MemberFunction.f_FindEqual(FunctionHash);
				
				if (!pEntry || pPublishedInterface->m_Hierarchy.f_BinarySearch(pEntry->m_TypeHash) < 0)
				{
					fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Function does not exist on remote actor"), Context);
					return true;
				}
				
				if (!pPublishedInterface->m_ProtocolVersions.f_ValidVersion(ProtocolVersion))
				{
					fp_ReplyToRemoteCallWithException(pHost, RemoteCall.m_PacketID, DMibErrorInstance("Protocol version is not valid for remote actor"), Context);
					return true;
				}

				if (ProtocolVersion < pEntry->m_LowestSupportedVersion)
				{
					fp_ReplyToRemoteCallWithException
						(
							pHost
							, RemoteCall.m_PacketID
							, DMibErrorInstance
							(
								NStr::fg_Format
								(
									"Tried to call function with protocol version 0x{nfh}, while the function requires protocol version 0x{nfh} or later on remote actor"
									, pEntry->m_LowestSupportedVersion
									, ProtocolVersion
								)
							)
							, Context
						)
					;
					return true;
				}

				fCall = [pEntry, pActor = NPrivate::fg_GetInternalActor(Actor)](CDistributedActorReadStream &_Stream) mutable
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
			while (false)
				;
		}
		
		NContainer::CSecureByteVector ParamData;
		{
			mint nBytes = _Stream.f_GetLength() - _Stream.f_GetPosition();
			if (nBytes != 0)
			{
				ParamData.f_SetLen(nBytes);
				_Stream.f_ConsumeBytes(ParamData.f_GetArray(), nBytes);
			}
		}

		Actor
			(
				&CActor::f_DispatchWithReturn<NConcurrency::TCFuture<NContainer::CSecureByteVector>>
				, 
				[
					Actor
					, ParamData = fg_Move(ParamData)
					, HostID = Host.m_HostInfo.m_RealHostID
					, CallingHostInfo = CCallingHostInfo
						(
							fg_ThisActor(m_pThis)
							, Host.m_AuthenticationHandler
							, Host.m_HostInfo.m_UniqueHostID
							, Host.f_GetHostInfo()
							, Host.m_LastExecutionID
							, ProtocolVersion
							, Host.m_ClaimedUserID
							, Host.m_ClaimedUserName
							, pHost
						)
					, Context
					, fCall = fg_Move(fCall)
					, ProtocolVersion
				] 
				() mutable
				{
					CCallingHostInfoScope CallingHostInfoScope{fg_Move(CallingHostInfo)};

					CDistributedActorReadStream Stream;
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
				, LastExecutionID = Host.m_LastExecutionID
			]
			(TCAsyncResult<NContainer::CSecureByteVector> &&_Result) mutable
			{
				if (LastExecutionID != pHost->m_LastExecutionID)
					return;
				
				if (!_Result)
				{
					fp_ReplyToRemoteCallWithException(pHost, PacketID, _Result, Context);
					return;
				}
				
				bool bException = (*_Result)[0] != 0;
				
				if (!bException)
				{
					TCPromise<> Promise;
					NStr::CStr Error;
					if (!Context.f_ValidateContext(Error))
					{
						Promise.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
						fp_ReplyToRemoteCallWithException(pHost, PacketID, Promise.f_MoveResult(), Context);
						return;
					}
					
					if (!fp_RegisterActorFunctorsForCall(*Context.m_pState, *pHost, Promise))
					{
						fp_ReplyToRemoteCallWithException(pHost, PacketID, Promise.f_MoveResult(), Context);
						return;
					}
				}

				fp_ReplyToRemoteCall(pHost, PacketID, *_Result, Context);
			}
		;
		return true;
	}

	void CActorDistributionManagerInternal::fp_RegisterImplicitSubscriptions
		(
			CHost &_Host
			, NPrivate::CDistributedActorStreamContextState &_State
			, CResultSubscriptionData const *_pSubscriptionData
		)
	{
		NContainer::TCVector<NStr::CStr> Claimed;
		if (_pSubscriptionData)
		{
			Claimed = _pSubscriptionData->m_ClaimedSubscriptionIDs;
			Claimed.f_Sort();
		}
		for (auto &Subscription : _State.m_Subscriptions)
		{
			if (_pSubscriptionData && _Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed) >= 0x104 && Claimed.f_BinarySearch(Subscription.m_SubscriptionID) < 0)
				continue;
			auto &PublishedSubscription = _Host.m_ImplicitlyPublishedSubscriptions[Subscription.m_SubscriptionID];
			PublishedSubscription.m_Subscription = fg_Move(Subscription.m_Subscription);
			PublishedSubscription.m_ReferencedActors = fg_Move(Subscription.m_ActorReferences); 
		}
	}
	
	void CActorDistributionManagerInternal::fp_RegisterLocalSubscriptions(NPrivate::CDistributedActorStreamContextState &_State)
	{
		auto &State = _State;
		auto pHost = State.m_pHost;
		if (!pHost)
			return;
		auto &Host = *pHost;
		if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || Host.m_LastExecutionID != State.m_LastExecutionID)
			return;
		
		for (auto &Subscription : State.m_Subscriptions)
		{
			uint32 SequenceID = State.m_Subscriptions.fs_GetKey(Subscription);

			auto &References = Host.m_LocalSubscriptionReferences[Subscription.m_SubscriptionID];
		
			auto *pActorFunctors = State.m_ActorFunctors.f_FindEqual(SequenceID);
			if (!pActorFunctors)
				continue;
			References.m_Actors = fg_Move(pActorFunctors->m_ImplicitlyPublishedActors);
			References.m_Interfaces = fg_Move(pActorFunctors->m_ImplicitlyPublishedInterfaces);
			References.m_Functions = fg_Move(pActorFunctors->m_ImplicitlyPublishedFunctions);
		}
	}

	void CActorDistributionManager::fp_RegisterRemoteSubscription
		(
			NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &_pContextState
			, NStr::CStr const &_SubscriptionID 
			, uint32 _SubscriptionSequenceID
		)
	{
		auto &State = *_pContextState;
		auto pHost = State.m_pHost;
		if (!pHost)
			return;
		auto &Host = *pHost;
		if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || Host.m_LastExecutionID != State.m_LastExecutionID)
			return;

		auto &References = Host.m_RemoteSubscriptionReferences[_SubscriptionID];

		auto *pActorFunctors = State.m_ActorFunctors.f_FindEqual(_SubscriptionSequenceID);
		if (!pActorFunctors)
			return;
		
		References.m_Actors = fg_Move(pActorFunctors->m_ImplicitlyPublishedActors);
		References.m_Interfaces = fg_Move(pActorFunctors->m_ImplicitlyPublishedInterfaces);
		References.m_Functions = fg_Move(pActorFunctors->m_ImplicitlyPublishedFunctions);
	}
	
	TCFuture<void> CActorDistributionManager::fp_DestroyRemoteSubscription
		(
			NStorage::TCSharedPointerSupportWeak<NPrivate::ICHost> const &_pHost
			, NStr::CStr const &_SubscriptionID
			, NStr::CStr const &_LastExecutionID
		)
	{
		auto &Host = *(static_cast<NActorDistributionManagerInternal::CHost *>(_pHost.f_Get()));
		{
			if (Host.m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || Host.m_LastExecutionID != _LastExecutionID)
				co_return {};
			auto *pSubscription = Host.m_RemoteSubscriptionReferences.f_FindEqual(_SubscriptionID);
			if (!pSubscription)
				co_return {}; // Host destroyed

			uint32 HostActorProtocolVersion = Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed);

			if (HostActorProtocolVersion >= 0x105)
			{
				auto pPendingDestroy = Host.m_PendingRemoteSubscriptionDestroys.f_FindEqual(_SubscriptionID);
				if (pPendingDestroy)
					co_return DMibErrorInstance("This subscription has already been destroyed");
			}

			for (auto &FunctionID : pSubscription->m_Functions)
				Host.f_DestroyImplicitFunction(FunctionID);
			for (auto &ActorID : pSubscription->m_Actors)
				Host.m_ImplicitlyPublishedActors.f_Remove(ActorID);
			for (auto &ActorID : pSubscription->m_Interfaces)
				Host.m_ImplicitlyPublishedInterfaces.f_Remove(ActorID);

			CDistributedActorCommand_DestroySubscription Result;
			Result.m_SubscriptionID = _SubscriptionID;

			auto &Internal = *mp_pInternal;

			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
			Stream << Result;
			Internal.fp_QueuePacket(fg_Explicit(&Host), Stream.f_MoveVector());

			if (HostActorProtocolVersion < 0x105)
				co_return {};
		}
		
		co_return co_await Host.m_PendingRemoteSubscriptionDestroys[_SubscriptionID].f_Future();
	}

	void CActorDistributionManagerInternal::fp_DestroyRemoteSubscriptionReset(NActorDistributionManagerInternal::CHost &_Host, NStr::CStr const &_SubscriptionID)
	{
		auto *pSubscription = _Host.m_RemoteSubscriptionReferences.f_FindEqual(_SubscriptionID);
		if (!pSubscription)
			return;

		for (auto &FunctionID : pSubscription->m_Functions)
			_Host.f_DestroyImplicitFunction(FunctionID);
		for (auto &ActorID : pSubscription->m_Actors)
			_Host.m_ImplicitlyPublishedActors.f_Remove(ActorID);
		for (auto &ActorID : pSubscription->m_Interfaces)
			_Host.m_ImplicitlyPublishedInterfaces.f_Remove(ActorID);

		_Host.m_RemoteSubscriptionReferences.f_Remove(pSubscription);
	}

	void CActorDistributionManagerInternal::fp_DestroyLocalSubscription(NActorDistributionManagerInternal::CHost &_Host, NStr::CStr const &_SubscriptionID)
	{
		auto *pSubscription = _Host.m_LocalSubscriptionReferences.f_FindEqual(_SubscriptionID);
		if (!pSubscription)
			return;
		
		for (auto &FunctionID : pSubscription->m_Functions)
			_Host.f_DestroyImplicitFunction(FunctionID);
		for (auto &ActorID : pSubscription->m_Actors)
			_Host.m_ImplicitlyPublishedActors.f_Remove(ActorID);
		for (auto &ActorID : pSubscription->m_Interfaces)
			_Host.m_ImplicitlyPublishedInterfaces.f_Remove(ActorID);
		
		_Host.m_LocalSubscriptionReferences.f_Remove(pSubscription);
	}
	
	bool CActorDistributionManagerInternal::fp_HandleDestroySubscription(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		if (pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;
		
		auto &Host = *pHost;

		CDistributedActorCommand_DestroySubscription DestroySubscription;
		_Stream >> DestroySubscription;
		uint32 ActorProtocolVersion = Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed);
		if (ActorProtocolVersion >= 0x105)
		{
			auto *pSubscription = Host.m_ImplicitlyPublishedSubscriptions.f_FindEqual(DestroySubscription.m_SubscriptionID);
			
			if (pSubscription && pSubscription->m_Subscription)
			{
				pSubscription->m_Subscription->f_Destroy()
					> [this, pHost, SubscriptionID = DestroySubscription.m_SubscriptionID, LastExecutionID = pHost->m_LastExecutionID, ActorProtocolVersion]
					(TCAsyncResult<void> &&_Result)
					{
						if (pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed) || pHost->m_LastExecutionID != LastExecutionID)
							return;

						CDistributedActorCommand_SubscriptionDestroyed Result;
						Result.m_SubscriptionID = SubscriptionID;
						Result.m_Result = fg_Move(_Result);

						NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
						Result.f_Feed(Stream, ActorProtocolVersion);
						fp_QueuePacket(pHost, Stream.f_MoveVector());
					}
				;
			}
			else
			{
				CDistributedActorCommand_SubscriptionDestroyed Result;
				Result.m_SubscriptionID = DestroySubscription.m_SubscriptionID;
				Result.m_Result.f_SetResult();

				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
				Result.f_Feed(Stream, ActorProtocolVersion);
				fp_QueuePacket(pHost, Stream.f_MoveVector());
			}
		}

		Host.m_ImplicitlyPublishedSubscriptions.f_Remove(DestroySubscription.m_SubscriptionID);
		fp_DestroyLocalSubscription(Host, DestroySubscription.m_SubscriptionID);
		
		return true;
	}

	bool CActorDistributionManagerInternal::fp_HandleSubscriptionDestroyed(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
	{
		auto &pHost = _pConnection->m_pHost;
		if (pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
			return true;

		auto &Host = *pHost;

		uint32 ActorProtocolVersion = Host.m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed);

		CDistributedActorCommand_SubscriptionDestroyed DestroySubscription;
		DestroySubscription.f_Consume(_Stream, ActorProtocolVersion);

		auto *pDestroy = Host.m_PendingRemoteSubscriptionDestroys.f_FindEqual(DestroySubscription.m_SubscriptionID);
		if (pDestroy)
		{
			pDestroy->f_SetResult(fg_Move(DestroySubscription.m_Result));
			Host.m_PendingRemoteSubscriptionDestroys.f_Remove(pDestroy);
		}
		
		return true;
	}
}

