// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CCommand>
	void CActorDistributionManagerInternal::fp_QueueCommand(NStorage::TCSharedPointerSupportWeak<CHost> const &_pHost, tf_CCommand const &_Command)
	{
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		Stream << _Command;
		fp_QueuePacket(_pHost, Stream.f_MoveVector());
	}
}
