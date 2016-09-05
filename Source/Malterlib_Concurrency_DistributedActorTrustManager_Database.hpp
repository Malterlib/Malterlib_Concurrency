// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NDistributedActorTrustManagerDatabase
		{
			template <typename tf_CStream>
			void CBasicConfig::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_HostID;
				_Stream << m_CAPrivateKey;
				_Stream << m_CACertificate;
			}
			
			template <typename tf_CStream>
			void CBasicConfig::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid basic config version");
				_Stream >> m_HostID;
				_Stream >> m_CAPrivateKey;
				_Stream >> m_CACertificate;
			}

			template <typename tf_CStream>
			void CServerCertificate::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_PrivateKey;
				_Stream << m_PublicCertificate;
			}
			
			template <typename tf_CStream>
			void CServerCertificate::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid server certificate version");
				_Stream >> m_PrivateKey;
				_Stream >> m_PublicCertificate;
			}

			template <typename tf_CStream>
			void CListenConfig::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_Address;
			}
			
			template <typename tf_CStream>
			void CListenConfig::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid listen config version");
				_Stream >> m_Address;
			}
			
			bool CListenConfig::operator == (CListenConfig const &_Right) const
			{
				return m_Address == _Right.m_Address;
			}
			
			bool CListenConfig::operator < (CListenConfig const &_Right) const
			{
				return m_Address < _Right.m_Address;
			}

			template <typename tf_CStream>
			void CClient::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_PublicCertificate;
				_Stream << m_LastFriendlyName;
			}
				
			template <typename tf_CStream>
			void CClient::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid client version");
				_Stream >> m_PublicCertificate;
				if (Version >= 0x102) 
					_Stream >> m_LastFriendlyName;
			}

			template <typename tf_CStream>
			void CClientConnection::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_PublicServerCertificate;
				_Stream << m_PublicClientCertificate;
				_Stream << m_LastFriendlyName;
			}
			
			template <typename tf_CStream>
			void CClientConnection::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid client connection version");
				_Stream >> m_PublicServerCertificate;
				_Stream >> m_PublicClientCertificate;
				if (Version >= 0x102) 
					_Stream >> m_LastFriendlyName;
			}
			
			template <typename tf_CStream>
			void CNamespace::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_AllowedHosts;
			}
			
			template <typename tf_CStream>
			void CNamespace::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid namespace version");
				_Stream >> m_AllowedHosts;
			}
			
			template <typename tf_CStream>
			void CHostPermissions::f_Feed(tf_CStream &_Stream) const
			{
				_Stream << EVersion;
				_Stream << m_Permissions;
			}
			
			template <typename tf_CStream>
			void CHostPermissions::f_Consume(tf_CStream &_Stream)
			{
				uint32 Version;
				_Stream >> Version;
				if (Version > EVersion)
					DMibError("Invalid host permissions version");
				_Stream >> m_Permissions;
			}
		}
	}
}
