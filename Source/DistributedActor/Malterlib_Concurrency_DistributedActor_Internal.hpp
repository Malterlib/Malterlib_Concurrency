// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NActorDistributionManagerInternal
{
	// Pack priority (8 bits) and packetID (56 bits) into a single uint64 for efficient lookup
	// Format: [priority:8][packetID:56] - priority in high bits for correct sort order
	inline_always uint64 fg_MakePriorityPacketKey(uint8 _Priority, uint64 _PacketID)
	{
		return (uint64(_Priority) << 56) | (_PacketID & 0x00FFFFFFFFFFFFFF);
	}

	// Validate that a packet ID from the network fits in 56 bits
	// Returns false if the ID has upper bits set (potential malicious client)
	inline_always bool fg_IsValidPacketID(uint64 _PacketID)
	{
		return (_PacketID & 0xFF00000000000000) == 0;
	}

	inline_always_debug uint64 CPacket::CSortPriorityPacketID::operator () (CPacket const &_Entry)
	{
		return fg_MakePriorityPacketKey(_Entry.m_Priority, _Entry.m_PacketID);
	}

	// Composite key for outstanding calls: encodes priority in upper 8 bits, packet ID in lower 56 bits
	inline_always uint64 fg_MakeOutstandingCallKey(uint8 _Priority, uint64 _PacketID)
	{
		return (uint64(_Priority) << 56) | (_PacketID & 0x00FFFFFFFFFFFFFFull);
	}

	inline_always_debug auto &CPublishedActor::CSortActorID::operator () (CPublishedActor const &_Entry)
	{
		return _Entry.f_GetActorID();
	}
}
