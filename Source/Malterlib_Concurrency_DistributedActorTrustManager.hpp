// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void CDistributedActorTrustManager::CTrustTicket::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			_Stream << m_ServerAddress;
			_Stream << m_ServerPublicCert;
			_Stream << m_Token;
		}
		
		template <typename tf_CStream>
		void CDistributedActorTrustManager::CTrustTicket::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid trust ticket version");
			_Stream >> m_ServerAddress;
			_Stream >> m_ServerPublicCert;
			_Stream >> m_Token;
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
