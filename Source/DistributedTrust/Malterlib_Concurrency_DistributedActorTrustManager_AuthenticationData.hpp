// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CAuthenticationData::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Category;
		_Stream % m_Name;
		_Stream % m_PublicData;
		_Stream % m_PrivateData;
	}
}

