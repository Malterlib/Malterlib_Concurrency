// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_DatabaseJSONDirectory.h"

#include <Mib/Encoding/EJSON>
#include <Mib/Encoding/JSONShortcuts>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Web/HTTP/URL>

namespace NMib::NConcurrency
{
	using namespace NDistributedActorTrustManagerDatabase;
	using namespace NStr;
	using namespace NEncoding;

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

		CInternal(CStr const &_BaseDirectory);

		CStr m_BaseDirectory;

		bool m_bGotBasicConfig = false;

		template <typename ...tfp_CComponent>
		CStr f_GetPath(tfp_CComponent const &...p_Component) const;
		template <typename ...tfp_CComponent>
		bool f_Delete(tfp_CComponent const &...p_Component) const;
		CStr f_GetNameHash(CDistributedActorTrustManager_Address const &_Object) const;
		CStr f_PercentEncode(CStr const &_Str) const;
		CStr f_PercentDecode(CStr const &_Str) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_Find(bool _bRecursive, tfp_CComponent const &...p_Component) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_Find(tfp_CComponent const &...p_Component) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_FindRecursive(tfp_CComponent const &...p_Component) const;
		template <typename ...tfp_CComponent>
		bool f_Exists(tfp_CComponent const &...p_Component) const;
		template <typename tf_CObject, typename ...tfp_CComponent>
		bool f_Read(tf_CObject &_Object, tfp_CComponent const &...p_Component) const;
		template <typename tf_CObject, typename ...tfp_CComponent>
		void f_Write(tf_CObject const &_Object, tfp_CComponent const &...p_Component) const;

		CDistributedActorTrustManager_Address f_DecodeAddress(CEJSON const &_JSON, CStr const &_Name) const;
		void f_EncodeAddress(CEJSON &_JSON, CDistributedActorTrustManager_Address const &_Address) const;

		CEJSON f_ToJSON(CSerial const &_Serial) const;
		void f_FromJSON(CSerial &_Serial, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CBasicConfig const &_BasicConfig) const;
		void f_FromJSON(CBasicConfig &_BasicConfig, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CServerCertificate const &_Certificate) const;
		void f_FromJSON(CServerCertificate &_ServerCertificate, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CDefaultUser const &_DefaultUser) const;
		void f_FromJSON(CDefaultUser &_DefaultUser, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CClient const &_Client) const;
		void f_FromJSON(CClient &_Client, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CInternalClientConnection const &_ClientConnection) const;
		void f_FromJSON(CClientConnection &_ClientConnection, CEJSON const &_JSON, CStr const &_Name) const;
		void f_FromJSON(CInternalClientConnection &_ClientConnection, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CListenConfig const &_ListenConfig) const;
		void f_FromJSON(CListenConfig &_ListenConfig, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CNamespace const &_Namespace) const;
		void f_FromJSON(CNamespace &_Namespace, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CPermissions const &_Permissions) const;
		void f_FromJSON(CPermissions &_Permissions, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CUserInfo const &_UserInfo) const;
		void f_FromJSON(CUserInfo &_UserInfo, CEJSON const &_JSON, CStr const &_Name) const;
		CEJSON f_ToJSON(CUserAuthenticationFactor const &_Factor) const;
		void f_FromJSON(CUserAuthenticationFactor &_Factor, CEJSON const &_JSON, CStr const &_Name) const;

		void f_DoConversion(uint32 _OldVersion);
		void f_CheckState();
	};

	CDistributedActorTrustManagerDatabase_JSONDirectory::CDistributedActorTrustManagerDatabase_JSONDirectory(CStr const &_BaseDirectory)
		: mp_pInternal(fg_Construct(_BaseDirectory))
	{
	}

	CDistributedActorTrustManagerDatabase_JSONDirectory::~CDistributedActorTrustManagerDatabase_JSONDirectory()
	{
		mp_pInternal.f_Clear();
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_CheckState()
	{
		if (!m_bGotBasicConfig)
			DMibError("You have to call f_GetBasicConfig() before doing any other operations");
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_DoConversion(uint32 _OldVersion)
	{
		if (_OldVersion < 0x101)
		{
			NContainer::TCMap<CPermissionIdentifiers, CPermissions> Permissions;
			for (auto &FileName : f_Find("HostPermissions"))
			{
				CPermissionIdentifiers Key = CPermissionIdentifiers::fs_GetKeyFromFileNameOld(FileName);
				auto &Permission = Permissions[Key];
				f_Read(Permission, "HostPermissions", FileName);
			}

			for (auto &Permission : Permissions)
				f_Write(Permission, "Permissions", Permissions.fs_GetKey(Permission).f_GetStr());

			CStr HostPermissionsDirectory = m_BaseDirectory / "HostPermissions";
			if (NFile::CFile::fs_FileExists(HostPermissionsDirectory))
				NFile::CFile::fs_DeleteDirectoryRecursive(HostPermissionsDirectory);
		}
	}

	TCFuture<CBasicConfig> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetBasicConfig()
	{
		return TCFuture<CBasicConfig>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				CBasicConfig BasicConfig;
				if (Internal.f_Exists("BasicConfig"))
				{
					BasicConfig.m_Version = 0;
					Internal.f_Read(BasicConfig, "BasicConfig");
				}

				if (BasicConfig.m_Version > CBasicConfig::EConversionVersion)
					DMibError("Invalid conversion version 0x{nfh} > 0x{nfh}"_f << BasicConfig.m_Version << CBasicConfig::EConversionVersion);
				else if (BasicConfig.m_Version < CBasicConfig::EConversionVersion)
				{
					Internal.f_DoConversion(BasicConfig.m_Version);
					BasicConfig.m_Version = CBasicConfig::EConversionVersion;
					Internal.f_Write(BasicConfig, "BasicConfig");
				}
				Internal.m_bGotBasicConfig = true;
				return BasicConfig;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetBasicConfig(CBasicConfig const &_BasicConfig)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				Internal.f_Write(_BasicConfig, "BasicConfig");
			}
		;
	}

	TCFuture<int32> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetNewCertificateSerial()
	{
		return TCFuture<int32>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CInternal::CSerial Serial;
				Internal.f_Read(Serial, "CertificateSerial");
				int32 ReturnSerial = Serial.m_Serial++;
				Internal.f_Write(Serial, "CertificateSerial");
				return ReturnSerial;
			}
		;
	}

	TCFuture<CDefaultUser> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetDefaultUser()
	{
		return TCFuture<CDefaultUser>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CDefaultUser DefaultUser;
				Internal.f_Read(DefaultUser, "DefaultUser");
				return DefaultUser;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetDefaultUser(CDefaultUser const &_DefaultUser)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				Internal.f_Write(_DefaultUser, "DefaultUser");
			}
		;
	}

	TCFuture<NContainer::TCMap<CStr, CServerCertificate>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumServerCertificates(bool _bIncludeFullInfo)
	{
		return TCFuture<NContainer::TCMap<CStr, CServerCertificate>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();

				NContainer::TCMap<CStr, CServerCertificate> ReturnCertificates;
				for (auto &FileName : Internal.f_Find("ServerCertificates"))
					Internal.f_Read(ReturnCertificates[Internal.f_PercentDecode(FileName)], "ServerCertificates", FileName);

				return ReturnCertificates;
			}
		;
	}

	TCFuture<CServerCertificate> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetServerCertificate(CStr const &_HostName)
	{
		return TCFuture<CServerCertificate>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CServerCertificate ServerCertificate;
				CStr FileName = Internal.f_PercentEncode(_HostName);
				if (!Internal.f_Read(ServerCertificate, "ServerCertificates", FileName))
					DMibError("No server certificate for host");
				return ServerCertificate;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CStr FileName = Internal.f_PercentEncode(_HostName);
				if (Internal.f_Exists("ServerCertificates", FileName))
					DMibError("Server certificate already exists");
				Internal.f_Write(_Certificate, "ServerCertificates", FileName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetServerCertificate(CStr const &_HostName, CServerCertificate const &_Certificate)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CStr FileName = Internal.f_PercentEncode(_HostName);
				if (!Internal.f_Exists("ServerCertificates", FileName))
					DMibError("No server certificate for host");
				Internal.f_Write(_Certificate, "ServerCertificates", FileName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveServerCertificate(CStr const &_HostName)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CStr FileName = Internal.f_PercentEncode(_HostName);
				if (!Internal.f_Delete("ServerCertificates", FileName))
					DMibError("No server certificate for host");
			}
		;
	}

	TCFuture<NContainer::TCSet<CListenConfig>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumListenConfigs()
	{
		return TCFuture<NContainer::TCSet<CListenConfig>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
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

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddListenConfig(CListenConfig const &_Config)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				auto NameHash = Internal.f_GetNameHash(_Config.m_Address);
				if (Internal.f_Exists("ListenConfigs", NameHash))
					DMibError("Listen config already exists");
				Internal.f_Write(_Config, "ListenConfigs", NameHash);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveListenConfig(CListenConfig const &_Config)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				auto NameHash = Internal.f_GetNameHash(_Config.m_Address);
				if (!Internal.f_Delete("ListenConfigs", NameHash))
					DMibError("No such listen config");
			}
		;
	}

	TCFuture<NContainer::TCMap<CStr, CClient>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumClients(bool _bIncludeFullInfo)
	{
		return TCFuture<NContainer::TCMap<CStr, CClient>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				NContainer::TCMap<CStr, CClient> Clients;
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

	TCFuture<CClient> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetClient(CStr const &_HostID)
	{
		return TCFuture<CClient>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();

				CClient Client;
				if (!Internal.f_Read(Client, "Clients", _HostID))
					DMibError("No client for host ID");
				return Client;
			}
		;
	}

	TCFuture<NStorage::TCUniquePointer<CClient>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_TryGetClient(CStr const &_HostID)
	{
		return TCFuture<NStorage::TCUniquePointer<CClient>>::fs_RunProtected<NException::CException>() /
			[&]() -> NStorage::TCUniquePointer<CClient>
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();

				CClient Client;
				if (!Internal.f_Read(Client, "Clients", _HostID))
					return nullptr;
				return fg_Construct(fg_Move(Client));
			}
		;
	}

	TCFuture<bool> CDistributedActorTrustManagerDatabase_JSONDirectory::f_HasClient(CStr const &_HostID)
	{
		return TCFuture<bool>::fs_RunProtected<NException::CException>() / [&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				return Internal.f_Exists("Clients", _HostID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddClient(CStr const &_HostID, CClient const &_Client)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (Internal.f_Exists("Clients", _HostID))
					DMibError("Client already exists");
				Internal.f_Write(_Client, "Clients", _HostID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetClient(CStr const &_HostID, CClient const &_Client)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Clients", _HostID))
					DMibError("No client for host ID");
				Internal.f_Write(_Client, "Clients", _HostID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveClient(CStr const &_HostID)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("Clients", _HostID))
					DMibError("No client for host ID");
			}
		;
	}

	auto CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumClientConnections(bool _bIncludeFullInfo)
		-> TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>>
	{
		return TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
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

	TCFuture<CClientConnection> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		return TCFuture<CClientConnection>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CClientConnection ClientConnection;
				if (!Internal.f_Read(ClientConnection, "ClientConnections", Internal.f_GetNameHash(_Address)))
					DMibError("No client connection for address");
				return ClientConnection;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddClientConnection
		(
			CDistributedActorTrustManager_Address const &_Address
			, CClientConnection const &_ClientConnection
		)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CStr NameHash = Internal.f_GetNameHash(_Address);
				if (Internal.f_Exists("ClientConnections", NameHash))
					DMibError("Client connection already exists");
				CInternal::CInternalClientConnection ClientConnection;
				ClientConnection.m_Address = _Address;
				ClientConnection.m_ClientConnection = _ClientConnection;
				Internal.f_Write(ClientConnection, "ClientConnections", NameHash);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetClientConnection
		(
			CDistributedActorTrustManager_Address const &_Address
			, CClientConnection const &_ClientConnection
		)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CEJSON JSON;

				CStr NameHash = Internal.f_GetNameHash(_Address);
				if (!Internal.f_Exists("ClientConnections", NameHash))
					DMibError("No client connection for address");
				CInternal::CInternalClientConnection ClientConnection;
				ClientConnection.m_Address = _Address;
				ClientConnection.m_ClientConnection = _ClientConnection;
				Internal.f_Write(ClientConnection, "ClientConnections", NameHash);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("ClientConnections", Internal.f_GetNameHash(_Address)))
					DMibError("No client connection for address");
			}
		;
	}

	TCFuture<NContainer::TCMap<CStr, CNamespace>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumNamespaces(bool _bIncludeFullInfo)
	{
		return TCFuture<NContainer::TCMap<CStr, CNamespace>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				NContainer::TCMap<CStr, CNamespace> Namespaces;
				for (auto &NamespaceName : Internal.f_Find("Namespaces"))
				{
					CStr DecodedNamespaceName = NamespaceName.f_ReplaceChar('_', '/');
					auto &Namespace = Namespaces[DecodedNamespaceName];
					if (_bIncludeFullInfo)
						Internal.f_Read(Namespace, "Namespaces", NamespaceName);

				}
				return Namespaces;
			}
		;
	}

	TCFuture<CNamespace> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetNamespace(CStr const &_NamespaceName)
	{
		return TCFuture<CNamespace>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();

				if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
					DMibError("Invalid namespace name");
				CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

				CNamespace Namespace;
				if (!Internal.f_Read(Namespace, "Namespaces", NamespaceName))
					DMibError("No namespace with that name");
				return Namespace;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
					DMibError("Invalid namespace name");
				CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (Internal.f_Exists("Namespaces", NamespaceName))
					DMibError("Namespace already exists");
				Internal.f_Write(_Namespace, "Namespaces", NamespaceName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetNamespace(CStr const &_NamespaceName, CNamespace const &_Namespace)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
					DMibError("Invalid namespace name");
				CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Namespaces", NamespaceName))
					DMibError("No namespace with that name");
				Internal.f_Write(_Namespace, "Namespaces", NamespaceName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveNamespace(CStr const &_NamespaceName)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
					DMibError("Invalid namespace name");
				CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("Namespaces", NamespaceName))
					DMibError("No namespace with that name");
			}
		;
	}

	TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumPermissions(bool _bIncludeFullInfo)
	{
		return TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				NContainer::TCMap<CPermissionIdentifiers, CPermissions> Permissions;
				for (auto &FileName : Internal.f_Find("Permissions"))
				{
					CPermissionIdentifiers Key = CPermissionIdentifiers::fs_GetKeyFromFileName(FileName);
					auto &Permission = Permissions[Key];
					if (_bIncludeFullInfo)
						Internal.f_Read(Permission, "Permissions", FileName);
				}
				return Permissions;
			}
		;
	}

	TCFuture<CPermissions> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetPermissions(CPermissionIdentifiers const &_Identity)
	{
		return TCFuture<CPermissions>::fs_RunProtected<NException::CException>() /
			[&]
			{
				CStr FileName = _Identity.f_GetStr();
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();

				CPermissions Permissions;
				if (!Internal.f_Read(Permissions, "Permissions", FileName))
					DMibError("No host permissions for that identity");
				return Permissions;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				CStr FileName = _Identity.f_GetStr();
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (Internal.f_Exists("Permissions", FileName))
					DMibError("Host permissions already exists for that identity");
				Internal.f_Write(_Permissions, "Permissions", FileName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetPermissions(CPermissionIdentifiers const &_Identity, CPermissions const &_Permissions)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				CStr FileName = _Identity.f_GetStr();
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Permissions", FileName))
					DMibError("No host permissions for that identity");
				Internal.f_Write(_Permissions, "Permissions", FileName);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemovePermissions(CPermissionIdentifiers const &_Identity)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				CStr FileName = _Identity.f_GetStr();
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("Permissions", FileName))
					DMibError("No host permissions for that identity");
			}
		;
	}

	TCFuture<NContainer::TCMap<CStr, CUserInfo>> CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumUsers(bool _bIncludeFullInfo)
	{
		return TCFuture<NContainer::TCMap<CStr, CUserInfo>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				NContainer::TCMap<CStr, CUserInfo> AllUsers;
				for (auto &UserID : Internal.f_Find("Users"))
				{
					CUserInfo UserInfo;
					Internal.f_Read(UserInfo, "Users", UserID);
					AllUsers[UserID] = UserInfo;
				}
				return AllUsers;
			}
		;
	}

	TCFuture<CUserInfo> CDistributedActorTrustManagerDatabase_JSONDirectory::f_GetUserInfo(CStr const &_UserID)
	{
		return TCFuture<CUserInfo>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				CUserInfo UserInfo;
				if (!Internal.f_Read(UserInfo, "Users", _UserID))
					DMibError("No information found for that user ID");
				return UserInfo;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddUser(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (Internal.f_Exists("Users", _UserID))
					DMibError("User info already exists for that user ID");
				Internal.f_Write(_UserInfo, "Users", _UserID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetUserInfo(CStr const &_UserID, CUserInfo const &_UserInfo)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Users", _UserID))
					DMibError("User '{}' does not exists"_f << _UserID);
				Internal.f_Write(_UserInfo, "Users", _UserID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveUser(CStr const &_UserID)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("Users", _UserID))
					DMibError("No user info for that user ID");
			}
		;
	}

	auto CDistributedActorTrustManagerDatabase_JSONDirectory::f_EnumAuthenticationFactor
		(
			bool _bIncludeFullInfo
		)
		-> TCFuture<NContainer::TCMap<CStr, NContainer::TCMap<CStr, CUserAuthenticationFactor>>>
	{
		return TCFuture<NContainer::TCMap<CStr, NContainer::TCMap<CStr, CUserAuthenticationFactor>>>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				NContainer::TCMap<CStr, NContainer::TCMap<CStr, CUserAuthenticationFactor>> AllFactors;
				for (auto &FactorIDFactor : Internal.f_FindRecursive("AuthenticationFactors"))
				{
					CUserAuthenticationFactor Factor;
					Internal.f_Read(Factor, "AuthenticationFactors", FactorIDFactor);
					if (!_bIncludeFullInfo)
						Factor.m_PrivateData.f_Clear();
					AllFactors[NFile::CFile::fs_GetPath(FactorIDFactor)][NFile::CFile::fs_GetFile(FactorIDFactor)] = Factor;
				}
				return AllFactors;
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_AddUserAuthenticationFactor
		(
			CStr const &_UserID
			, CStr const &_FactorID
			, CUserAuthenticationFactor const &_Factor
		)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Users", _UserID))
					DMibError("User '{}' does not exists"_f << _UserID);
				if (Internal.f_Exists("AuthenticationFactors", _UserID, _FactorID))
					DMibError("User '{}' already has authentication factor ID '{}' registered"_f << _UserID << _FactorID);
				Internal.f_Write(_Factor, "AuthenticationFactors", _UserID, _FactorID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_SetUserAuthenticationFactor
		(
			CStr const &_UserID
			, CStr const &_FactorID
			, CUserAuthenticationFactor const &_Factor
		)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Exists("Users", _UserID))
					DMibError("User '{}' does not exists"_f << _UserID);
				if (!Internal.f_Exists("AuthenticationFactors", _UserID, _FactorID))
					DMibError("User '{}' Does not have authentication factor ID '{}' registered"_f << _UserID << _FactorID);
				Internal.f_Write(_Factor, "AuthenticationFactors", _UserID, _FactorID);
			}
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JSONDirectory::f_RemoveUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID)
	{
		return TCFuture<void>::fs_RunProtected<NException::CException>() /
			[&]
			{
				auto &Internal = *mp_pInternal;
				Internal.f_CheckState();
				if (!Internal.f_Delete("AuthenticationFactors", _UserID, _FactorID))
					DMibError("No authentication factor '{}' registered for user ID '{}'"_f << _FactorID << _UserID);
			}
		;
	}

	CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::CInternal(CStr const &_BaseDirectory)
		: m_BaseDirectory(_BaseDirectory)
	{
	}

	template <typename ...tfp_CComponent>
	CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_GetPath(tfp_CComponent const &...p_Component) const
	{
		CStr Return = m_BaseDirectory;

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
		CStr Path = f_GetPath(p_Component...);
		if (!NFile::CFile::fs_FileExists(Path))
			return false;
		NFile::CFile::fs_DeleteFile(Path);
		return true;
	}

	CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_GetNameHash(CDistributedActorTrustManager_Address const &_Object) const
	{
		NCryptography::TCBinaryStreamHash<NCryptography::CHash_SHA256> HashStream;
		_Object.m_URL.f_Feed(HashStream, 0x101);
		auto Digest = HashStream.f_GetDigest();
		return Digest.f_GetString();
	}

	CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_PercentEncode(CStr const &_Str) const
	{
		CStr Encoded;
		NWeb::NHTTP::CURL::fs_PercentEncode(Encoded, _Str);
		return Encoded;
	}

	CStr CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_PercentDecode(CStr const &_Str) const
	{
		CStr Decoded;
		NWeb::NHTTP::CURL::fs_PercentDecode(Decoded, _Str);
		return Decoded;
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Find(bool _bRecursive, tfp_CComponent const &...p_Component) const
	{
		using namespace NFile;
		CStr Path = f_GetPath(p_Component..., "*");

		CFile::CFindFilesOptions Options(Path, _bRecursive);
		Options.m_AttribMask = EFileAttrib_File;
		auto Files = CFile::fs_FindFiles(Options);

		NContainer::TCSet<CStr> ReturnNames;
		CStr const SearchPath = CFile::fs_GetPath(Path);
		for (auto const &File : Files)
		{
			ReturnNames[CFile::fs_AppendPath(CFile::fs_MakePathRelative(CFile::fs_GetPath(File.m_Path), SearchPath), CFile::fs_GetFileNoExt(File.m_Path))];
		}
		return ReturnNames;
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Find(tfp_CComponent const &...p_Component) const
	{
		return f_Find(false, p_Component...);
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FindRecursive(tfp_CComponent const &...p_Component) const
	{
		return f_Find(true, p_Component...);
	}

	template <typename ...tfp_CComponent>
	bool CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Exists(tfp_CComponent const &...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);
		return NFile::CFile::fs_FileExists(Path);
	}

	template <typename tf_CObject, typename ...tfp_CComponent>
	bool CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Read(tf_CObject &o_Object, tfp_CComponent const &...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);
		if (!NFile::CFile::fs_FileExists(Path))
			return false;

		f_FromJSON(o_Object, CEJSON::fs_FromString(NFile::CFile::fs_ReadStringFromFile(Path)), Path);
		return true;
	}

	template <typename tf_CObject, typename ...tfp_CComponent>
	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_Write(tf_CObject const &_Object, tfp_CComponent const &...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);

		NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(Path));
		// Only allow the user to read and write these files as they contain private keys
		NFile::EFileAttrib Attributes = NFile::EFileAttrib_UnixAttributesValid | NFile::EFileAttrib_UserRead | NFile::EFileAttrib_UserWrite;
		CStr TempFile = Path + ".tmp";
		NFile::CFile::fs_WriteStringToFile(TempFile, f_ToJSON(_Object).f_ToString(), true, Attributes);
		if (NFile::CFile::fs_FileExists(Path))
			NFile::CFile::fs_AtomicReplaceFile(TempFile, Path);
		else
			NFile::CFile::fs_RenameFile(TempFile, Path);
	}

	CDistributedActorTrustManager_Address CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_DecodeAddress(CEJSON const &_JSON, CStr const &_Name) const
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _JSON.f_String();

		CStr FileName = NFile::CFile::fs_GetFileNoExt(_Name);

		if (f_GetNameHash(Address) != FileName)
			DMibError(fg_Format("Address does not match the hashed file name '{}'", _Name));

		return Address;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_EncodeAddress(CEJSON &o_JSON, CDistributedActorTrustManager_Address const &_Address) const
	{
		o_JSON = _Address.m_URL.f_Encode();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CSerial const &_Serial) const
	{
		CEJSON JSON;
		JSON["Serial"] = _Serial.m_Serial;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CSerial &o_Serial, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_Serial.m_Serial = _JSON["Serial"].f_Integer();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CBasicConfig const &_BasicConfig) const
	{
		CEJSON JSON;
		JSON["HostID"] = _BasicConfig.m_HostID;
		JSON["CAPrivateKey"] = NContainer::CByteVector{_BasicConfig.m_CAPrivateKey};
		JSON["CACertificate"] = _BasicConfig.m_CACertificate;
		JSON["Version"] = _BasicConfig.m_Version;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CBasicConfig &o_BasicConfig, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_BasicConfig.m_HostID = _JSON["HostID"].f_String();
		if (auto *pValue = _JSON.f_GetMember("CAPrivateKey"))
			o_BasicConfig.m_CAPrivateKey = pValue->f_Binary();
		if (auto *pValue = _JSON.f_GetMember("CACertificate"))
			o_BasicConfig.m_CACertificate = pValue->f_Binary();
		if (auto *pValue = _JSON.f_GetMember("Version"))
			o_BasicConfig.m_Version = pValue->f_Integer();
		else
			o_BasicConfig.m_Version = 0;
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CDefaultUser const &_DefaultUser) const
	{
		CEJSON JSON;
		JSON["UserID"] = _DefaultUser.m_UserID;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CDefaultUser &o_DefaultUser, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_DefaultUser.m_UserID = _JSON["UserID"].f_String();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CServerCertificate const &_Certificate) const
	{
		CEJSON JSON;
		JSON["PrivateKey"] = NContainer::CByteVector{_Certificate.m_PrivateKey};
		JSON["PublicCertificate"] = _Certificate.m_PublicCertificate;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CServerCertificate &o_ServerCertificate, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_ServerCertificate.m_PrivateKey = _JSON["PrivateKey"].f_Binary();
		o_ServerCertificate.m_PublicCertificate = _JSON["PublicCertificate"].f_Binary();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CClient const &_Client) const
	{
		CEJSON JSON;
		JSON["PublicCertificate"] = _Client.m_PublicCertificate;
		JSON["LastFriendlyName"] = _Client.m_LastFriendlyName;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CClient &o_Client, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_Client.m_PublicCertificate = _JSON["PublicCertificate"].f_Binary();
		if (auto *pName = _JSON.f_GetMember("LastFriendlyName"))
			o_Client.m_LastFriendlyName = pName->f_String();
		else
			o_Client.m_LastFriendlyName.f_Clear();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CInternalClientConnection const &_ClientConnection) const
	{
		CEJSON JSON;
		f_EncodeAddress(JSON["Address"], _ClientConnection.m_Address);
		JSON["PublicServerCertificate"] = _ClientConnection.m_ClientConnection.m_PublicServerCertificate;
		JSON["PublicClientCertificate"] = _ClientConnection.m_ClientConnection.m_PublicClientCertificate;
		JSON["LastFriendlyName"] = _ClientConnection.m_ClientConnection.m_LastFriendlyName;
		JSON["ConnectionConcurrency"] = _ClientConnection.m_ClientConnection.m_ConnectionConcurrency;
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CClientConnection &o_ClientConnection, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_ClientConnection.m_PublicServerCertificate = _JSON["PublicServerCertificate"].f_Binary();
		o_ClientConnection.m_PublicClientCertificate = _JSON["PublicClientCertificate"].f_Binary();
		if (auto *pName = _JSON.f_GetMember("LastFriendlyName"))
			o_ClientConnection.m_LastFriendlyName = pName->f_String();
		else
			o_ClientConnection.m_LastFriendlyName.f_Clear();
		if (auto *pName = _JSON.f_GetMember("ConnectionConcurrency"))
			o_ClientConnection.m_ConnectionConcurrency = pName->f_Integer();
		else
			o_ClientConnection.m_ConnectionConcurrency = -1;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CInternalClientConnection &o_ClientConnection, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_ClientConnection.m_Address = f_DecodeAddress(_JSON["Address"], _Name);
		f_FromJSON(o_ClientConnection.m_ClientConnection, _JSON, _Name);
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CListenConfig const &_ListenConfig) const
	{
		CEJSON JSON;
		f_EncodeAddress(JSON["Address"], _ListenConfig.m_Address);
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CListenConfig &o_ListenConfig, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_ListenConfig.m_Address = f_DecodeAddress(_JSON["Address"], _Name);
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CNamespace const &_Namespace) const
	{
		CEJSON JSON;
		auto &AllowedHosts = JSON["AllowedHosts"].f_Array();
		for (auto &AllowedHost : _Namespace.m_AllowedHosts)
			AllowedHosts.f_Insert(AllowedHost);
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CNamespace &o_Namespace, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_Namespace.m_AllowedHosts.f_Clear();
		for (auto &AllowedHost : _JSON["AllowedHosts"].f_Array())
			o_Namespace.m_AllowedHosts[AllowedHost.f_String()];
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CPermissions const &_Permissions) const
	{
		CEJSON JSON;
		auto &PermissionsObject = JSON["Permissions"] = EJSONType_Object;
		for (auto &AuthenticationFactors : _Permissions.m_Permissions)
		{
			auto &RequiredFactors = PermissionsObject[_Permissions.m_Permissions.fs_GetKey(AuthenticationFactors)] = EJSONType_Object;
			RequiredFactors["MaximumAuthenticationLifetime"] = AuthenticationFactors.m_MaximumAuthenticationLifetime;
			auto &Factors = RequiredFactors["RequiredAuthenticationFactors"].f_Array();
			for (auto const &InnerSet : AuthenticationFactors.m_AuthenticationFactors)
			{
				CEJSON Array = EJSONType_Array;
				for (auto const &Factor : InnerSet)
					Array.f_Insert(Factor);
				Factors.f_Insert(Array);
			}
		}
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CPermissions &o_Permissions, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_Permissions.m_Permissions.f_Clear();
		auto &PermissionsObject = _JSON["Permissions"];
		if (PermissionsObject.f_IsObject())
		{
			for (auto &Permission : PermissionsObject.f_Object())
			{
				CPermissionRequirements AuthenticationFactors;
				auto &FactorsObject = Permission.f_Value().f_Object();
				if (auto pValue = FactorsObject.f_GetMember("MaximumAuthenticationLifetime"))
					AuthenticationFactors.m_MaximumAuthenticationLifetime = pValue->f_Integer();

				auto pValue = FactorsObject.f_GetMember("RequiredAuthenticationFactors");
				for (auto &Inner :  pValue->f_Array())
				{
					NContainer::TCSet<CStr> Factors;
					for (auto &Factor : Inner.f_Array())
						Factors[Factor.f_String()];
					AuthenticationFactors.m_AuthenticationFactors[Factors];
				}
				o_Permissions.m_Permissions[Permission.f_Name()] = fg_Move(AuthenticationFactors);
			}
		}
		else
		{
			for (auto &Permission : PermissionsObject.f_Array())
				o_Permissions.m_Permissions[Permission.f_String()];
		}
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CUserInfo const &_UserInfo) const
	{
		CEJSON JSON;
		JSON["UserName"] = _UserInfo.m_UserName;
		if (!_UserInfo.m_Metadata.f_IsEmpty())
		{
			auto &Metadata = JSON["Metadata"] = EJSONType_Object;
			for (auto &Item : _UserInfo.m_Metadata)
			{
				Metadata[_UserInfo.m_Metadata.fs_GetKey(Item)] = Item;
			}
		}
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CUserInfo &o_UserInfo, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_UserInfo.m_UserName = _JSON["UserName"].f_String();
		o_UserInfo.m_Metadata.f_Clear();
		auto pValue = _JSON.f_GetMember("Metadata");
		if (!pValue)
			return;

		auto Object = pValue->f_Object();
		for (auto iMetadata = Object.f_OrderedIterator(); iMetadata; ++iMetadata)
			o_UserInfo.m_Metadata[iMetadata->f_Name()] = iMetadata->f_Value();
	}

	CEJSON CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_ToJSON(CUserAuthenticationFactor const &_Factor) const
	{
		CEJSON JSON;
		JSON["Category"] = _Factor.m_Category;
		JSON["Name"] = _Factor.m_Name;
		if (!_Factor.m_PublicData.f_IsEmpty())
		{
			auto &PublicData = JSON["PublicData"] = EJSONType_Object;
			for (auto &Item : _Factor.m_PublicData)
				PublicData[_Factor.m_PublicData.fs_GetKey(Item)] = Item;
		}
		if (!_Factor.m_PrivateData.f_IsEmpty())
		{
			auto &PrivateData = JSON["PrivateData"] = EJSONType_Object;
			for (auto &Item : _Factor.m_PrivateData)
				PrivateData[_Factor.m_PrivateData.fs_GetKey(Item)] = Item;
		}
		return JSON;
	}

	void CDistributedActorTrustManagerDatabase_JSONDirectory::CInternal::f_FromJSON(CUserAuthenticationFactor &o_Factor, CEJSON const &_JSON, CStr const &_Name) const
	{
		o_Factor.m_Category = (EAuthenticationFactorCategory)_JSON["Category"].f_Integer();
		o_Factor.m_Name = _JSON["Name"].f_String();
		o_Factor.m_PublicData.f_Clear();
		o_Factor.m_PrivateData.f_Clear();

		if (auto pValue = _JSON.f_GetMember("PublicData"))
		{
			auto Object = pValue->f_Object();
			for (auto iPublicData = Object.f_OrderedIterator(); iPublicData; ++iPublicData)
				o_Factor.m_PublicData[iPublicData->f_Name()] = iPublicData->f_Value();
		}

		if (auto pValue = _JSON.f_GetMember("PrivateData"))
		{
			auto Object = pValue->f_Object();
			for (auto iPrivateData = Object.f_OrderedIterator(); iPrivateData; ++iPrivateData)
				o_Factor.m_PrivateData[iPrivateData->f_Name()] = iPrivateData->f_Value();
		}
	}
}
