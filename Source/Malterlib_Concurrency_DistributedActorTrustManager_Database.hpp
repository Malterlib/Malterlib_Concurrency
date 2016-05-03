// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CBasicConfig::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			
			_Stream << m_HostID;
			_Stream << m_Subject;

			_Stream << m_CAPrivateKey;
			_Stream << m_CACertificate;
		}
		
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CBasicConfig::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid basic config version");
			
			_Stream >> m_HostID;
			_Stream >> m_Subject;

			_Stream >> m_CAPrivateKey;
			_Stream >> m_CACertificate;
		}

		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CServerCertificate::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			
			_Stream << m_PrivateKey;
			_Stream << m_PublicCertificate;
		}
		
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CServerCertificate::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid server certificate version");
			
			_Stream >> m_PrivateKey;
			_Stream >> m_PublicCertificate;
		}

		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CListenConfig::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			
			_Stream << m_Address;
		}
		
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CListenConfig::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid listen config version");
			
			_Stream >> m_Address;
		}
		
		bool ICDistributedActorTrustManagerDatabase::CListenConfig::operator == (CListenConfig const &_Right) const
		{
			return m_Address == _Right.m_Address;
		}
		
		bool ICDistributedActorTrustManagerDatabase::CListenConfig::operator < (CListenConfig const &_Right) const
		{
			return m_Address < _Right.m_Address;
		}

		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CClient::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			
			_Stream << m_PublicCertificate;
		}
			
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CClient::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid client version");
			
			_Stream >> m_PublicCertificate;
		}

		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CClientConnection::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << EVersion;
			
			_Stream << m_PublicServerCertificate;
			_Stream << m_PublicClientCertificate;
		}
		
		template <typename tf_CStream>
		void ICDistributedActorTrustManagerDatabase::CClientConnection::f_Consume(tf_CStream &_Stream)
		{
			uint32 Version;
			_Stream >> Version;
			if (Version > EVersion)
				DMibError("Invalid client connection version");
			
			_Stream >> m_PublicServerCertificate;
			_Stream >> m_PublicClientCertificate;
		}
	}
}
