// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_Interface.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>

namespace NMib::NConcurrency
{
	NStr::CStr CDistributedActorTrustManagerInterface::CTrustTicket::f_ToStringTicket() const
	{
		auto Data = NStream::fg_ToByteVector(*this);
		return NDataProcessing::fg_Base64Encode(Data);
	}
	
	CDistributedActorTrustManagerInterface::CTrustTicket CDistributedActorTrustManagerInterface::CTrustTicket::fs_FromStringTicket(NStr::CStr const &_StringTicket)
	{
		NContainer::TCVector<uint8> Data;
		NDataProcessing::fg_Base64Decode(_StringTicket, Data);
		return NStream::fg_FromByteVector<CTrustTicket>(Data);
	}
}
