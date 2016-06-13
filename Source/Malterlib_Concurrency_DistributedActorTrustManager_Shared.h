// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Web/HTTP/URL>

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedActorTrustManager_Address
		{
			template <typename tf_CStream>
			void f_Feed(tf_CStream &_Stream) const;
			template <typename tf_CStream>
			void f_Consume(tf_CStream &_Stream);
			bool operator == (CDistributedActorTrustManager_Address const &_Right) const;
			bool operator < (CDistributedActorTrustManager_Address const &_Right) const;
			
			NHTTP::CURL m_URL;
			
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_IntoStr) const;
		};
	}
}

#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.hpp"
