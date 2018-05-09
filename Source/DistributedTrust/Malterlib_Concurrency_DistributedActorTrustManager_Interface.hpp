// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CTrustTicket::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << EVersion;
		_Stream << m_ServerAddress;
		_Stream << m_ServerPublicCert;
		_Stream << m_Token;
	}
	
	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CTrustTicket::f_Consume(tf_CStream &_Stream)
	{
		uint32 Version;
		_Stream >> Version;
		if (Version > EVersion)
			DMibError("Invalid trust ticket version");
		_Stream >> m_ServerAddress;
		_Stream >> m_ServerPublicCert;
		_Stream >> m_Token;
	}

	inline CDistributedActorTrustManagerInterface::CLocalCallingHostInfo::CLocalCallingHostInfo(CCallingHostInfo &&_Other)
		: CCallingHostInfo(fg_Move(_Other))
	{

	}

	inline CDistributedActorTrustManagerInterface::CLocalCallingHostInfo::CLocalCallingHostInfo(CCallingHostInfo const &_Other)
		: CCallingHostInfo(_Other)
	{
	}

	NStr::CStr const &CDistributedActorTrustManagerInterface::CNamespacePermissions::f_GetName() const
	{
		return NContainer::TCMap<NStr::CStr, CNamespacePermissions>::fs_GetKey(*this);
	}
	
	bool CDistributedActorTrustManagerInterface::CNamespacePermissions::operator ==(CNamespacePermissions const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_AllowedHosts, m_DisallowedHosts) == NContainer::fg_TupleReferences(_Right.m_AllowedHosts, _Right.m_DisallowedHosts); 
	}
	
	bool CDistributedActorTrustManagerInterface::CNamespacePermissions::operator <(CNamespacePermissions const &_Right) const
	{
		return NContainer::fg_TupleReferences(m_AllowedHosts, m_DisallowedHosts) < NContainer::fg_TupleReferences(_Right.m_AllowedHosts, _Right.m_DisallowedHosts); 
	}

	template <typename tf_CString>
	void CDistributedActorTrustManagerInterface::CNamespacePermissions::f_Format(tf_CString &o_String) const
	{
		o_String += typename tf_CString::CFormat("Allowed {vs,vb} Disallowed {vs,vb}") << m_AllowedHosts << m_DisallowedHosts;
	}
	
	template <typename tf_CString>
	void CDistributedActorTrustManagerInterface::CClientConnectionInfo::f_Format(tf_CString &o_String) const
	{
		o_String += typename tf_CString::CFormat("{} ({} concurrency)") << m_HostInfo << m_ConnectionConcurrency;
	}

	template <typename tf_CStream>
	void CDistributedActorTrustManagerInterface::CUserInfo::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_UserName;
		_Stream % m_Metadata;
	}

	template <typename tf_CString>
	void CDistributedActorTrustManagerInterface::CUserInfo::f_Format(tf_CString &o_String) const
	{
		o_String += typename tf_CString::CFormat("User : '{}', Metadata '{}')") << m_UserName << m_Metadata;
	}
}
