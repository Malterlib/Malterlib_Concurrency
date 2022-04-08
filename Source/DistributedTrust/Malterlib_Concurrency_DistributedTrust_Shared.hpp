// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
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

	template <typename tf_CStream>
	void CPermissionIdentifier::f_Stream(tf_CStream &_Stream)
	{
		// Waring: If you change streaming here you need to version in remote interfaces
		_Stream % m_Type;
		_Stream % m_ID;
	}

	template <typename tf_CStr>
	void CPermissionIdentifier::f_Format(tf_CStr &o_Str) const
	{
		switch (m_Type)
		{
		case EPermissionIdentifierType_HostID:
			o_Str += typename tf_CStr::CFormat("HostID: {}") << m_ID;
			break;

		case EPermissionIdentifierType_UserID:
			o_Str += typename tf_CStr::CFormat("UserID: {}") << m_ID;
			break;

		default:
			o_Str += typename tf_CStr::CFormat("Unhandled ID type: {}") << m_ID;
			break;
		}
	}

	template <typename tf_CStream>
	void CPermissionIdentifiers::f_Stream(tf_CStream &_Stream)
	{
		// Waring: If you change streaming here you need to version in remote interfaces
		_Stream % m_IDs;
	}

	template <typename tf_CStr>
	void CPermissionIdentifiers::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}") << m_IDs;
	}

	template <typename tf_CStr>
	void CPermissionRequirements::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{vs} lifetime {}") << m_AuthenticationFactors << m_MaximumAuthenticationLifetime;
	}
}
