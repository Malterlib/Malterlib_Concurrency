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
			_Stream << m_URL;
			_Stream << m_PreferType;
		}
		
		template <typename tf_CStream>
		void CDistributedActorTrustManager_Address::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_URL;
			_Stream >> m_PreferType;
		}
		
		template <typename tf_CStr>
		void CDistributedActorTrustManager_Address::f_Format(tf_CStr &o_IntoStr) const
		{
			o_IntoStr += typename tf_CStr::CFormat("{} {}") << m_URL.f_Encode() << m_PreferType;
		}
	}
}
