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
		CDistributedActorPublication::CDistributedActorPublication(CDistributedActorPublication &&) = default;
		CDistributedActorPublication &CDistributedActorPublication::operator = (CDistributedActorPublication &&) = default;
			
		CDistributedActorPublication::CDistributedActorPublication(TCActor<CActorDistributionManager> _DistributionManager, NStr::CStr const &_Namespace, NStr::CStr const &_ActorID)
			: mp_DistributionManager(_DistributionManager)
			, mp_Namespace(_Namespace) 
			, mp_ActorID(_ActorID)
		{
		}

		CDistributedActorPublication::CDistributedActorPublication() = default;

		CDistributedActorPublication::~CDistributedActorPublication()
		{
			f_Clear();
		}
		
		void CDistributedActorPublication::f_Clear()
		{
			if (mp_DistributionManager)
			{
				mp_DistributionManager
					(
						&CActorDistributionManager::fp_RemoveActorPublication
						, mp_Namespace
						, mp_ActorID
					)
					> fg_DiscardResult()
				;
				
				mp_DistributionManager.f_Clear();
				mp_Namespace.f_Clear();
				mp_ActorID.f_Clear();
			}
		}
		
		TCContinuation<CDistributedActorPublication> CActorDistributionManager::f_PublishActor
			(
				TCDistributedActor<CActor> &&_Actor
				, NStr::CStr const &_Namespace
				, CDistributedActorInheritanceHeirarchyPublish const &_ClassesToPublish
			)
		{
			TCContinuation<CDistributedActorPublication> Continuation;
			auto &Internal = *mp_pInternal;
			auto &LocalNamespace = Internal.m_LocalNamespaces[_Namespace];
			auto pDistributedActorData = static_cast<NPrivate::CDistributedActorData const *>(_Actor->f_GetDistributedActorData().f_Get());
			DMibRequire(pDistributedActorData);

			auto &PublishedActor = LocalNamespace.m_Actors[pDistributedActorData->m_ActorID];
			
			if (PublishedActor.m_Actor)
			{
				Continuation.f_SetException(DMibErrorInstance("This actor has already been published in this namespace"));
				return Continuation;
			}
			Internal.m_PublishedActors.f_Insert(PublishedActor);
			PublishedActor.m_Actor = _Actor;
			PublishedActor.m_Hierarchy = _ClassesToPublish.f_GetHierarchy();
			PublishedActor.m_Hierarchy.f_Sort();
			
			CDistributedActorCommand_Publish Publish;
			Publish.m_ActorID = pDistributedActorData->m_ActorID;
			Publish.m_Namespace = _Namespace;
			Publish.m_Hierarchy = PublishedActor.m_Hierarchy; 

			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Publish;
			auto Data = Stream.f_MoveVector();
			
			for (auto &Host : Internal.m_Hosts)
			{
				if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
					continue;
				
				Internal.fp_QueuePacket(&Host, fg_TempCopy(Data));
			}

			Continuation.f_SetResult(CDistributedActorPublication(fg_ThisActor(this), _Namespace, pDistributedActorData->m_ActorID));
			
			return Continuation;
		}

		void CActorDistributionManager::fp_RemoveActorPublication(NStr::CStr const &_Namespace, NStr::CStr const &_ActorID)
		{
			auto &Internal = *mp_pInternal;
			auto *pLocalNamespace = Internal.m_LocalNamespaces.f_FindEqual(_Namespace);
			DMibRequire(pLocalNamespace);
			auto *pPublishedActor = pLocalNamespace->m_Actors.f_FindEqual(_ActorID);
			DMibRequire(pPublishedActor);
			
			Internal.m_PublishedActors.f_Remove(pPublishedActor);

			pLocalNamespace->m_Actors.f_Remove(pPublishedActor);
			
			if (pLocalNamespace->m_Actors.f_IsEmpty())
				Internal.m_LocalNamespaces.f_Remove(pLocalNamespace);
			
			CDistributedActorCommand_Unpublish Unpublish;
			Unpublish.m_ActorID = _ActorID;
			Unpublish.m_Namespace = _Namespace;
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << Unpublish;
			auto Data = Stream.f_MoveVector();
			
			for (auto &Host : Internal.m_Hosts)
			{
				if (!Host.m_bAllowAllNamespaces && !Host.m_AllowedNamespaces.f_FindEqual(_Namespace))
					continue;
				
				Internal.fp_QueuePacket(&Host, fg_TempCopy(Data));
			}
		}
		
		CAbstractDistributedActor::CAbstractDistributedActor(TCDistributedActor<CActor> const &_Actor, NContainer::TCVector<uint32> const &_InheritanceHierarchy)
			: mp_Actor(_Actor)
			, mp_InheritanceHierarchy(_InheritanceHierarchy)
		{
		}

		void CActorDistributionManager::CInternal::fp_NotifyNewActor(CHost *_pHost, CRemoteActor &_RemoteActor)
		{
			{
				auto &ConcurrencyManager = fg_ConcurrencyManager();

				NPtr::TCSharedPointer<CDistributedActorDataInternal> pDistributedActorData = fg_Construct();
				pDistributedActorData->m_pHost = _pHost;
				pDistributedActorData->m_bRemote = true;
				pDistributedActorData->m_ActorID = _RemoteActor.f_GetActorID();
				
				NPtr::TCSharedPointer<TCActorInternal<TCDistributedActorWrapper<CActor>>, NPtr::CSupportWeakTag, CInternalActorAllocator> pActor 
					= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData))
				;
				
				_RemoteActor.m_Actor = ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<CActor>>
					(
						fg_Move(pActor)
						, fg_Construct<TCDistributedActorWrapper<CActor>>()
					)
				;
			}
			
			CAbstractDistributedActor AbstractActor(_RemoteActor.m_Actor, _RemoteActor.m_Hierarchy);
			auto pAll = m_SubscribedActors.f_FindEqual("");
			if (pAll)
				pAll->m_fOnNewActor(fg_TempCopy(AbstractActor));
			
			auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
			if (pSpecific)
				pSpecific->m_fOnNewActor(fg_TempCopy(AbstractActor));
		}
		
		void CActorDistributionManager::CInternal::fp_NotifyRemovedActor(CRemoteActor const &_RemoteActor)
		{
			auto pAll = m_SubscribedActors.f_FindEqual("");
			if (pAll)
				pAll->m_fOnRemovedActorActor(_RemoteActor.m_Actor.f_Weak());
			
			auto pSpecific = m_SubscribedActors.f_FindEqual(_RemoteActor.m_Namespace);
			if (pSpecific)
				pSpecific->m_fOnRemovedActorActor(_RemoteActor.m_Actor.f_Weak());
		}
		
		bool CActorDistributionManager::CInternal::fp_HandlePublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
		{
			CDistributedActorCommand_Publish Publish;
			_Stream >> Publish;
			
			auto pHost = _pConnection->m_pHost;
			bool bAllowAllNamespaces = false;
			auto &AllowedNamespaces = fp_GetAllowedNamespacesForHost(pHost, bAllowAllNamespaces);
			
			if (!bAllowAllNamespaces && !AllowedNamespaces.f_FindEqual(Publish.m_Namespace))
				return true;
			
			auto Mapped = pHost->m_RemoteActors(Publish.m_ActorID);
			
			if (Mapped.f_WasCreated())
			{
				auto &RemoteActor = *Mapped;
				RemoteActor.m_Namespace = Publish.m_Namespace;
				RemoteActor.m_Hierarchy = Publish.m_Hierarchy;
				
				fp_NotifyNewActor(pHost, RemoteActor);
				
				m_RemoteNamespaces[Publish.m_Namespace].m_RemoteActors.f_Insert(RemoteActor);
			}
			
			return true;
		}
		
		bool CActorDistributionManager::CInternal::fp_HandleUnpublishPacket(CConnection *_pConnection, NStream::CBinaryStreamMemoryPtr<> &_Stream)
		{
			CDistributedActorCommand_Unpublish Unpublish;
			_Stream >> Unpublish;
			
			auto pHost = _pConnection->m_pHost;
			auto pRemoteActor = pHost->m_RemoteActors.f_FindEqual(Unpublish.m_ActorID);
			if (pRemoteActor)
			{
				fp_NotifyRemovedActor(*pRemoteActor);
				auto pRemoteNamespace = m_RemoteNamespaces.f_FindEqual(pRemoteActor->m_Namespace);
				DMibCheck(pRemoteNamespace);
				pRemoteNamespace->m_RemoteActors.f_Remove(pRemoteActor);
				if (pRemoteNamespace->m_RemoteActors.f_IsEmpty())
					m_RemoteNamespaces.f_Remove(pRemoteNamespace);
				
				pHost->m_RemoteActors.f_Remove(pRemoteActor);
			}
			
			return true;
		}

		CActorDistributionManager::CInternal::CActorSubscription::CActorSubscription(CActorDistributionManager *_pManager)
			: m_fOnNewActor(_pManager, false)
			, m_fOnRemovedActorActor(_pManager, false)
		{
		}

		CActorCallback CActorDistributionManager::f_SubscribeActors
			(
				NContainer::TCVector<NStr::CStr> const &_NameSpaces
				, TCActor<CActor> const &_Actor
				, NFunction::TCFunction<void (NFunction::CThisTag &, CAbstractDistributedActor &&_NewActor)> &&_fOnNewActor
				, NFunction::TCFunction<void (NFunction::CThisTag &, TCWeakDistributedActor<CActor> const &_RemovedActor)> &&_fOnRemovedActor
			)
		{
			auto &Internal = *mp_pInternal;
			NPtr::TCUniquePointer<CCombinedCallbackReference> pCallback = fg_Construct();
			if (_NameSpaces.f_IsEmpty())
			{
				auto &Subscribed = *Internal.m_SubscribedActors("", this);
				
				pCallback->m_References.f_Insert(Subscribed.m_fOnNewActor.f_Register(_Actor, fg_TempCopy(_fOnNewActor)));
				pCallback->m_References.f_Insert(Subscribed.m_fOnRemovedActorActor.f_Register(_Actor, fg_TempCopy(_fOnRemovedActor)));
				
				for (auto &RemoteNamespace : Internal.m_RemoteNamespaces)
				{
					for (auto &RemoteActor : RemoteNamespace.m_RemoteActors)
						_fOnNewActor(CAbstractDistributedActor(RemoteActor.m_Actor, RemoteActor.m_Hierarchy));
				}
			}
			else
			{
				for (auto &Namespace : _NameSpaces)
				{
					auto &Subscribed = *Internal.m_SubscribedActors(Namespace, this);
					
					pCallback->m_References.f_Insert(Subscribed.m_fOnNewActor.f_Register(_Actor, fg_TempCopy(_fOnNewActor)));
					pCallback->m_References.f_Insert(Subscribed.m_fOnRemovedActorActor.f_Register(_Actor, fg_TempCopy(_fOnRemovedActor)));
					if (auto *pRemoteNamespace = Internal.m_RemoteNamespaces.f_FindEqual(Namespace))
					{
						for (auto &RemoteActor : pRemoteNamespace->m_RemoteActors)
							_fOnNewActor(CAbstractDistributedActor(RemoteActor.m_Actor, RemoteActor.m_Hierarchy));
					}
				}
			}
			
			return fg_Move(pCallback);
		}
	}
}
