// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NActorDistributionManagerInternal
{
	CPacket::CPacket(NStorage::TCSharedPointer<NStream::CBinaryStorage const> const &_pData, uint8 _Priority, uint64 _PacketID)
		: m_pData(_pData)
		, m_PacketID(_PacketID)
		, m_Priority(_Priority)
	{
	}

	NStr::CStr const &CRemoteActor::f_GetActorID() const
	{
		return NContainer::TCMap<NStr::CStr, CRemoteActor>::fs_GetKey(this);
	}

	CRemoteActor::~CRemoteActor()
	{
		DMibFastCheck(!m_Link.f_IsInList());
	}

	inline_always_lto NStr::CStr const &CPublishedActor::f_GetActorID() const
	{
		return NContainer::TCMap<NStr::CStr, CPublishedActor>::fs_GetKey(this);
	}

	NStr::CStr const &CLocalNamespace::f_GetNamespace() const
	{
		return NContainer::TCMap<NStr::CStr, CLocalNamespace>::fs_GetKey(this);
	}

	umint CActorPublicationSubscriptionInstance::f_GetID() const
	{
		return NContainer::TCMap<umint, CActorPublicationSubscriptionInstance>::fs_GetKey(this);
	}

	NStr::CStr const &CListen::f_GetID() const
	{
		return NContainer::TCMap<NStr::CStr, CListen>::fs_GetKey(*this);
	}

	NStr::CStr const &CWebsocketHandler::f_GetWebPath() const
	{
		return NContainer::TCMap<NStr::CStr, CWebsocketHandler>::fs_GetKey(*this);
	}
}
