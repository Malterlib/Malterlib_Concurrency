// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void CDistributedActorTrustManager_Address::f_Feed(tf_CStream &_Stream) const
		{
			m_URL.f_Feed(_Stream, 0x101);
		}
		
		template <typename tf_CStream>
		void CDistributedActorTrustManager_Address::f_Consume(tf_CStream &_Stream)
		{
			m_URL.f_Consume(_Stream);
		}
		
		template <typename tf_CStr>
		void CDistributedActorTrustManager_Address::f_Format(tf_CStr &o_IntoStr) const
		{
			o_IntoStr += typename tf_CStr::CFormat("{}") << m_URL.f_Encode();
		}
	}
}
