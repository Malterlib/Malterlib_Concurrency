// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once


namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CCommand>
		void CActorDistributionManagerInternal::fp_QueueCommand(NPtr::TCSharedPointer<CHost, NPtr::CSupportWeakTag> const &_pHost, tf_CCommand const &_Command)
		{
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
			Stream << _Command;
			fp_QueuePacket(_pHost, Stream.f_MoveVector());
		}
	}
}
