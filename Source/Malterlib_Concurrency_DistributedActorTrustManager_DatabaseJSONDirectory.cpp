// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_DatabaseJSONDirectory.h"

#include <Mib/Encoding/EJSON>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Web/HTTP/URL>

namespace NMib
{
	namespace NConcurrency
	{
		using namespace NDistributedActorTrustManagerDatabase;
		
		struct CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal
		{
			using CActorHolder = CSeparateThreadActorHolder;

			struct CSerial
			{
				int32 m_Serial = 1;
			};
			
			struct CInternalClientConnection
			{
				CDistributedActorTrustManager_Address m_Address;
				CClientConnection m_ClientConnection;
			};
			
			CInternal(NStr::CStr const &_BaseDirectory);
			
			NStr::CStr m_BaseDirectory;
			
			template <typename ...tfp_CComponent>
			NStr::CStr f_GetPath(tfp_CComponent const &...p_Component) const;
			template <typename ...tfp_CComponent>
			bool f_Delete(tfp_CComponent const &...p_Component) const;
			template <typename tf_CObject>
			NStr::CStr f_GetNameHash(tf_CObject const &_Object) const;
			NStr::CStr f_PercentEncode(NStr::CStr const &_Str) const;
			NStr::CStr f_PercentDecode(NStr::CStr const &_Str) const;
			template <typename ...tfp_CComponent>
			NContainer::TCSet<NStr::CStr> f_Find(tfp_CComponent const &...p_Component) const;
			template <typename ...tfp_CComponent>
			bool f_Exists(tfp_CComponent const &...p_Component) const;
			template <typename tf_CObject, typename ...tfp_CComponent>
			bool f_Read(tf_CObject &_Object, tfp_CComponent const &...p_Component) const;
			template <typename tf_CObject, typename ...tfp_CComponent>
			void f_Write(tf_CObject const &_Object, tfp_CComponent const &...p_Component) const;
			
			CDistributedActorTrustManager_Address f_DecodeAddress(NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			void f_EncodeAddress(NEncoding::CEJSON &_JSON, CDistributedActorTrustManager_Address const &_Address) const;
			
			NEncoding::CEJSON f_ToJSON(CSerial const &_Serial) const;
			void f_FromJSON(CSerial &_Serial, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CBasicConfig const &_BasicConfig) const;
			void f_FromJSON(CBasicConfig &_BasicConfig, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CServerCertificate const &_Certificate) const;
			void f_FromJSON(CServerCertificate &_ServerCertificate, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CClient const &_Client) const;
			void f_FromJSON(CClient &_Client, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CInternalClientConnection const &_ClientConnection) const;
			void f_FromJSON(CClientConnection &_ClientConnection, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			void f_FromJSON(CInternalClientConnection &_ClientConnection, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CListenConfig const &_ListenConfig) const;
			void f_FromJSON(CListenConfig &_ListenConfig, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
			NEncoding::CEJSON f_ToJSON(CNamespace const &_Namespace) const;
			void f_FromJSON(CNamespace &_Namespace, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const;
		};
		
		CDistributedActorTrustManagerDatabase_JSONDirectory::CDistributedActorTrustManagerDatabase_JSONDirectory(NStr::CStr const &_BaseDirectory)
			: mp_pInternal(fg_Construct(_BaseDirectory)) 
		{
		}
		
		CDistributedActorTrustManagerDatabase_JSONDirectory::~CDistributedActorTrustManagerDatabase_JSONDirectory()
		{
			mp_pInternal.f_Clear();
		}
		
		TCContinuation<CBasicConfig> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetBasicConfig()
		{
			return TCContinuation<CBasicConfig>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					CBasicConfig BasicConfig;
					Internal.f_Read(BasicConfig, "BasicConfig");
					return BasicConfig;
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetBasicConfig(CBasicConfig const &_BasicConfig)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					Internal.f_Write(_BasicConfig, "BasicConfig");
				}
			;
		}
		
		TCContinuation<int32> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetNewCertificateSerial()
		{
			return TCContinuation<int32>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					CInternal::CSerial Serial;
					Internal.f_Read(Serial, "CertificateSerial");
					int32 ReturnSerial = Serial.m_Serial++;
					Internal.f_Write(Serial, "CertificateSerial");
					return ReturnSerial;
				}
			;
		}
		
		TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumServerCertificates(bool _bIncludeFullInfo)
		{
			return TCContinuation<NContainer::TCMap<NStr::CStr, CServerCertificate>>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					
					NContainer::TCMap<NStr::CStr, CServerCertificate> ReturnCertificates;
					for (auto &FileName : Internal.f_Find("ServerCertificates"))
						Internal.f_Read(ReturnCertificates[Internal.f_PercentDecode(FileName)], "ServerCertificates", FileName);
					
					return ReturnCertificates;
				}
			;
		}
		
		TCContinuation<CServerCertificate> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetServerCertificate(NStr::CStr const &_HostName)
		{
			return TCContinuation<CServerCertificate>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					CServerCertificate ServerCertificate;
					NStr::CStr FileName = Internal.f_PercentEncode(_HostName);
					if (!Internal.f_Read(ServerCertificate, "ServerCertificates", FileName))
						DMibError("No server certificate for host");
					return ServerCertificate;
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NStr::CStr FileName = Internal.f_PercentEncode(_HostName);
					if (Internal.f_Exists("ServerCertificates", FileName))
						DMibError("Server certificate already exists");
					Internal.f_Write(_Certificate, "ServerCertificates", FileName);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetServerCertificate(NStr::CStr const &_HostName, CServerCertificate const &_Certificate)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NStr::CStr FileName = Internal.f_PercentEncode(_HostName);
					if (!Internal.f_Exists("ServerCertificates", FileName))
						DMibError("No server certificate for host");
					Internal.f_Write(_Certificate, "ServerCertificates", FileName);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveServerCertificate(NStr::CStr const &_HostName)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NStr::CStr FileName = Internal.f_PercentEncode(_HostName);
					if (!Internal.f_Delete("ServerCertificates", FileName))
						DMibError("No server certificate for host");
				}
			;
		}
		
		TCContinuation<NContainer::TCSet<CListenConfig>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumListenConfigs()
		{
			return TCContinuation<NContainer::TCSet<CListenConfig>>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					auto FileNames = Internal.f_Find("ListenConfigs");
					
					NContainer::TCSet<CListenConfig> ListenConfigs;
					for (auto const &FileName : FileNames)
					{
						CListenConfig ListenConfig;
						if (!Internal.f_Read(ListenConfig, "ListenConfigs", FileName))
							DMibError("Internal error reading listen config");
						ListenConfigs[ListenConfig];
					}
					
					return ListenConfigs;
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddListenConfig(CListenConfig const &_Config)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					auto NameHash = Internal.f_GetNameHash(_Config.m_Address);
					if (Internal.f_Exists("ListenConfigs", NameHash))
						DMibError("Listen config already exists");
					Internal.f_Write(_Config, "ListenConfigs", NameHash);
				}
			;
		}
			
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveListenConfig(CListenConfig const &_Config)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					auto NameHash = Internal.f_GetNameHash(_Config.m_Address);
					if (!Internal.f_Delete("ListenConfigs", NameHash))
						DMibError("No such listen config");
				}
			;
		}

		TCContinuation<NContainer::TCMap<NStr::CStr, CClient>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumClients(bool _bIncludeFullInfo)
		{
			return TCContinuation<NContainer::TCMap<NStr::CStr, CClient>>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NContainer::TCMap<NStr::CStr, CClient> Clients;
					for (auto &HostID : Internal.f_Find("Clients"))
					{
						auto &Client = Clients[HostID];
						if (_bIncludeFullInfo)
							Internal.f_Read(Client, "Clients", HostID);
							
					}
					return Clients;
				}
			;
		}
		
		TCContinuation<CClient> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetClient(NStr::CStr const &_HostID)
		{
			return TCContinuation<CClient>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					
					CClient Client;
					if (!Internal.f_Read(Client, "Clients", _HostID))
						DMibError("No client for host ID");
					return Client;
				}
			;
		}
		
		TCContinuation<NPtr::TCUniquePointer<CClient>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_TryGetClient(NStr::CStr const &_HostID)
		{
			return TCContinuation<NPtr::TCUniquePointer<CClient>>::fs_RunProtected<NException::CException>() > 
				[&]() -> NPtr::TCUniquePointer<CClient>
				{
					auto &Internal = *mp_pInternal;
					
					CClient Client;
					if (!Internal.f_Read(Client, "Clients", _HostID))
						return nullptr;
					return fg_Construct(fg_Move(Client));
				}
			;
		}
		
		TCContinuation<bool> CDistributedActorTrustManagerDatabase_JSONDirectory::f_HasClient(NStr::CStr const &_HostID)
		{
			return TCContinuation<bool>::fs_RunProtected<NException::CException>() > [&]
				{
					auto &Internal = *mp_pInternal;
					return Internal.f_Exists("Clients", _HostID);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddClient(NStr::CStr const &_HostID, CClient const &_Client)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (Internal.f_Exists("Clients", _HostID))
						DMibError("Client already exists");
					Internal.f_Write(_Client, "Clients", _HostID);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetClient(NStr::CStr const &_HostID, CClient const &_Client)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (!Internal.f_Exists("Clients", _HostID))
						DMibError("No client for host ID");
					Internal.f_Write(_Client, "Clients", _HostID);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveClient(NStr::CStr const &_HostID)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (!Internal.f_Delete("Clients", _HostID))
						DMibError("No client for host ID");
				}
			;
		}		
		
		auto CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumClientConnections(bool _bIncludeFullInfo)
			-> TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>>
		{
			return TCContinuation<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					auto FileNames = Internal.f_Find("ClientConnections");
					
					NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> ClientConnections;
					for (auto const &FileName : FileNames)
					{
						CInternal::CInternalClientConnection ClientConnection;
						Internal.f_Read(ClientConnection, "ClientConnections", FileName);
						if (_bIncludeFullInfo)
							ClientConnections[ClientConnection.m_Address] = ClientConnection.m_ClientConnection;
						else
							ClientConnections[ClientConnection.m_Address];
					}
					
					return ClientConnections;
				}
			;
		}
		
		TCContinuation<CClientConnection> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address)
		{
			return TCContinuation<CClientConnection>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					CClientConnection ClientConnection;
					if (!Internal.f_Read(ClientConnection, "ClientConnections", Internal.f_GetNameHash(_Address)))
						DMibError("No client connection for address");
					return ClientConnection;
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddClientConnection
			(
				CDistributedActorTrustManager_Address const &_Address
				, CClientConnection const &_ClientConnection
			)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NStr::CStr NameHash = Internal.f_GetNameHash(_Address);
					if (Internal.f_Exists("ClientConnections", NameHash))
						DMibError("Client connection already exists");
					CInternal::CInternalClientConnection ClientConnection;
					ClientConnection.m_Address = _Address;
					ClientConnection.m_ClientConnection = _ClientConnection;
					Internal.f_Write(ClientConnection, "ClientConnections", NameHash);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetClientConnection
			(
				CDistributedActorTrustManager_Address const &_Address
				, CClientConnection const &_ClientConnection
			)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NEncoding::CEJSON JSON;
					
					NStr::CStr NameHash = Internal.f_GetNameHash(_Address);
					if (!Internal.f_Exists("ClientConnections", NameHash))
						DMibError("No client connection for address");
					CInternal::CInternalClientConnection ClientConnection;
					ClientConnection.m_Address = _Address;
					ClientConnection.m_ClientConnection = _ClientConnection;
					Internal.f_Write(ClientConnection, "ClientConnections", NameHash);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (!Internal.f_Delete("ClientConnections", Internal.f_GetNameHash(_Address)))
						DMibError("No client connection for address");
				}
			;
		}

		TCContinuation<NContainer::TCMap<NStr::CStr, CNamespace>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumNamespaces(bool _bIncludeFullInfo)
		{
			return TCContinuation<NContainer::TCMap<NStr::CStr, CNamespace>>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					NContainer::TCMap<NStr::CStr, CNamespace> Namespaces;
					for (auto &NamespaceName : Internal.f_Find("Namespaces"))
					{
						auto &Namespace = Namespaces[NamespaceName];
						if (_bIncludeFullInfo)
							Internal.f_Read(Namespace, "Namespaces", NamespaceName);
							
					}
					return Namespaces;
				}
			;
		}
		
		TCContinuation<CNamespace> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetNamespace(NStr::CStr const &_NamespaceName)
		{
			return TCContinuation<CNamespace>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					
					CNamespace Namespace;
					if (!Internal.f_Read(Namespace, "Namespaces", _NamespaceName))
						DMibError("No namespace with that name");
					return Namespace;
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (Internal.f_Exists("Namespaces", _NamespaceName))
						DMibError("Namespace already exists");
					Internal.f_Write(_Namespace, "Namespaces", _NamespaceName);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetNamespace(NStr::CStr const &_NamespaceName, CNamespace const &_Namespace)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (!Internal.f_Exists("Namespaces", _NamespaceName))
						DMibError("No namespace with that name");
					Internal.f_Write(_Namespace, "Namespaces", _NamespaceName);
				}
			;
		}
		
		TCContinuation<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveNamespace(NStr::CStr const &_NamespaceName)
		{
			return TCContinuation<void>::fs_RunProtected<NException::CException>() > 
				[&]
				{
					auto &Internal = *mp_pInternal;
					if (!Internal.f_Delete("Namespaces", _NamespaceName))
						DMibError("No namespace with that name");
				}
			;
		}		
		
		CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::CInternal(NStr::CStr const &_BaseDirectory)
			: m_BaseDirectory(_BaseDirectory)
		{
		}
		
		template <typename ...tfp_CComponent>
		NStr::CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_GetPath(tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Return = m_BaseDirectory;
			
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						Return += "/";
						Return += p_Component;
						return true;
					}
					()...
				}
			;
			(void)Dummy;
			
			Return += ".json";
			return Return;
		}
		
		template <typename ...tfp_CComponent>
		bool CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Delete(tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Path = f_GetPath(p_Component...);
			if (!NFile::CFile::fs_FileExists(Path))
				return false;
			NFile::CFile::fs_DeleteFile(Path);
			return true;
		}
		
		template <typename tf_CObject>
		NStr::CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_GetNameHash(tf_CObject const &_Object) const
		{
			NDataProcessing::TCBinaryStreamHash<NDataProcessing::CHash_SHA256> HashStream;
			HashStream << _Object;
			auto Digest = HashStream.f_GetDigest();
			return Digest.f_GetString();
		}
		
		NStr::CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_PercentEncode(NStr::CStr const &_Str) const
		{
			NStr::CStr Encoded;
			NHTTP::CURL::fs_PercentEncode(Encoded, _Str);
			return Encoded;
		}
		
		NStr::CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_PercentDecode(NStr::CStr const &_Str) const
		{
			NStr::CStr Decoded;
			NHTTP::CURL::fs_PercentDecode(Decoded, _Str);
			return Decoded;
		}

		template <typename ...tfp_CComponent>
		NContainer::TCSet<NStr::CStr> CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Find(tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Path = f_GetPath(p_Component..., "*");
			
			NFile::CFile::CFindFilesOptions Options(Path, false);
			Options.m_AttribMask = NFile::EFileAttrib_File;
			auto Files = NFile::CFile::fs_FindFiles(Options);
			
			NContainer::TCSet<NStr::CStr> ReturnNames;
			for (auto const &File : Files)
				ReturnNames[NFile::CFile::fs_GetFileNoExt(File.m_Path)];
			return ReturnNames;
		}

		template <typename ...tfp_CComponent>
		bool CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Exists(tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Path = f_GetPath(p_Component...);
			return NFile::CFile::fs_FileExists(Path);
		}
		
		template <typename tf_CObject, typename ...tfp_CComponent>
		bool CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Read(tf_CObject &o_Object, tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Path = f_GetPath(p_Component...);
			if (!NFile::CFile::fs_FileExists(Path))
				return false;
			
			f_FromJSON(o_Object, NEncoding::CEJSON::fs_FromString(NFile::CFile::fs_ReadStringFromFile(Path)), Path);
			return true;
		}

		template <typename tf_CObject, typename ...tfp_CComponent>
		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Write(tf_CObject const &_Object, tfp_CComponent const &...p_Component) const
		{
			NStr::CStr Path = f_GetPath(p_Component...);
			
			NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(Path));
			// Only allow the user to read and write these files as they contain private keys
			NFile::EFileAttrib Attributes = NFile::EFileAttrib_UnixAttributesValid | NFile::EFileAttrib_UserRead | NFile::EFileAttrib_UserWrite;
			NFile::CFile::fs_WriteStringToFile(Path, f_ToJSON(_Object).f_ToString(), true, Attributes);
		}
		
		CDistributedActorTrustManager_Address CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_DecodeAddress(NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			CDistributedActorTrustManager_Address Address;
			Address.m_URL = _JSON.f_String();
			
			NStr::CStr FileName = NFile::CFile::fs_GetFileNoExt(_Name);
			
			if (f_GetNameHash(Address) != FileName)
				DMibError(NStr::fg_Format("Address does not match the hashed file name '{}'", _Name));
			
			return Address;
		}

		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_EncodeAddress(NEncoding::CEJSON &o_JSON, CDistributedActorTrustManager_Address const &_Address) const
		{
			o_JSON = _Address.m_URL.f_Encode();
		}

		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CSerial const &_Serial) const
		{
			NEncoding::CEJSON JSON;
			JSON["Serial"] = _Serial.m_Serial;
			return JSON;
		}
		
		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CSerial &o_Serial, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_Serial.m_Serial = _JSON["Serial"].f_Integer();
		}

		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CBasicConfig const &_BasicConfig) const
		{
			NEncoding::CEJSON JSON;
			JSON["HostID"] = _BasicConfig.m_HostID;
			JSON["CAPrivateKey"] = NContainer::TCVector<uint8>{_BasicConfig.m_CAPrivateKey};
			JSON["CACertificate"] = _BasicConfig.m_CACertificate;
			return JSON;
		}
		
		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CBasicConfig &o_BasicConfig, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_BasicConfig.m_HostID = _JSON["HostID"].f_String();
			o_BasicConfig.m_CAPrivateKey = _JSON["CAPrivateKey"].f_Binary();
			o_BasicConfig.m_CACertificate = _JSON["CACertificate"].f_Binary();
		}
		
		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CServerCertificate const &_Certificate) const
		{
			NEncoding::CEJSON JSON;
			JSON["PrivateKey"] = NContainer::TCVector<uint8>{_Certificate.m_PrivateKey};
			JSON["PublicCertificate"] = _Certificate.m_PublicCertificate;
			return JSON;
		}
		
		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CServerCertificate &o_ServerCertificate, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_ServerCertificate.m_PrivateKey = _JSON["PrivateKey"].f_Binary();
			o_ServerCertificate.m_PublicCertificate = _JSON["PublicCertificate"].f_Binary();
		}
		
		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CClient const &_Client) const
		{
			NEncoding::CEJSON JSON;
			JSON["PublicCertificate"] = _Client.m_PublicCertificate;
			JSON["LastFriendlyName"] = _Client.m_LastFriendlyName;
			return JSON;
		}

		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CClient &o_Client, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_Client.m_PublicCertificate = _JSON["PublicCertificate"].f_Binary();
			if (auto *pName = _JSON.f_GetMember("LastFriendlyName"))
				o_Client.m_LastFriendlyName = pName->f_String();
			else
				o_Client.m_LastFriendlyName.f_Clear();			
		}
		
		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CInternalClientConnection const &_ClientConnection) const
		{
			NEncoding::CEJSON JSON;
			f_EncodeAddress(JSON["Address"], _ClientConnection.m_Address);
			JSON["PublicServerCertificate"] = _ClientConnection.m_ClientConnection.m_PublicServerCertificate;
			JSON["PublicClientCertificate"] = _ClientConnection.m_ClientConnection.m_PublicClientCertificate;
			JSON["LastFriendlyName"] = _ClientConnection.m_ClientConnection.m_LastFriendlyName;
			return JSON;
		}

		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CClientConnection &o_ClientConnection, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_ClientConnection.m_PublicServerCertificate = _JSON["PublicServerCertificate"].f_Binary();
			o_ClientConnection.m_PublicClientCertificate = _JSON["PublicClientCertificate"].f_Binary();
			if (auto *pName = _JSON.f_GetMember("LastFriendlyName"))
				o_ClientConnection.m_LastFriendlyName = pName->f_String();
			else
				o_ClientConnection.m_LastFriendlyName.f_Clear();			
		}
	
		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CInternalClientConnection &o_ClientConnection, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_ClientConnection.m_Address = f_DecodeAddress(_JSON["Address"], _Name);
			f_FromJSON(o_ClientConnection.m_ClientConnection, _JSON, _Name);
		}
		
		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CListenConfig const &_ListenConfig) const
		{
			NEncoding::CEJSON JSON;
			f_EncodeAddress(JSON["Address"], _ListenConfig.m_Address);
			return JSON;
		}

		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CListenConfig &o_ListenConfig, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_ListenConfig.m_Address = f_DecodeAddress(_JSON["Address"], _Name);
		}
		
		NEncoding::CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CNamespace const &_Namespace) const
		{
			NEncoding::CEJSON JSON;
			auto &AllowedHosts = JSON["AllowedHosts"].f_Array();
			for (auto &AllowedHost : _Namespace.m_AllowedHosts)
				AllowedHosts.f_Insert(AllowedHost);
			return JSON;
		}

		void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CNamespace &o_Namespace, NEncoding::CEJSON const &_JSON, NStr::CStr const &_Name) const
		{
			o_Namespace.m_AllowedHosts.f_Clear();
			for (auto &AllowedHost : _JSON["AllowedHosts"].f_Array())
				o_Namespace.m_AllowedHosts[AllowedHost.f_String()];
		}
	}
}
