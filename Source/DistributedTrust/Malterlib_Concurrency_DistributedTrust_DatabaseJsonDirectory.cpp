// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedTrust_DatabaseJsonDirectory.h"

#include <Mib/Encoding/EJson>
#include <Mib/Encoding/JsonShortcuts>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Concurrency/ActorSequencerActor>
#include <Mib/Concurrency/LogError>
#include <Mib/Web/HTTP/URL>

namespace NMib::NConcurrency
{
	using namespace NDistributedActorTrustManagerDatabase;
	using namespace NStr;
	using namespace NEncoding;

	// The files of the database. They are only read and written on the blocking actor, in the order of the sequencer
	// of CInternal. The lambdas dispatched there read the actor's state only through this, which is immutable.
	struct CJsonDirectoryStore
	{
		struct CPrimaryListen
		{
			CDistributedActorTrustManager_Address m_Address;
		};

		struct CSerial
		{
			int32 m_Serial = 1;
		};

		struct CInternalClientConnection
		{
			CDistributedActorTrustManager_Address m_Address;
			CClientConnection m_ClientConnection;
		};

		struct CInternalListenConfig
		{
			CDistributedActorTrustManager_Address m_Address;
			CListenConfig m_ListenConfig;
		};

		struct CInternalServerCertificate
		{
			CStr m_HostName;
			CServerCertificate m_ServerCertificate;
		};

		CJsonDirectoryStore(CStr const &_BaseDirectory);

		template <typename ...tfp_CComponent>
		CStr f_GetPath(tfp_CComponent const & ...p_Component) const;
		template <typename ...tfp_CComponent>
		bool f_Delete(tfp_CComponent const & ...p_Component) const;
		CStr f_GetNameHash(CDistributedActorTrustManager_Address const &_Object) const;
		CStr f_GetNameHash(CStr const &_Object) const;
		CStr f_PercentEncode(CStr const &_Str) const;
		CStr f_PercentDecode(CStr const &_Str) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_Find(bool _bRecursive, tfp_CComponent const & ...p_Component) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_Find(tfp_CComponent const & ...p_Component) const;
		template <typename ...tfp_CComponent>
		NContainer::TCSet<CStr> f_FindRecursive(tfp_CComponent const & ...p_Component) const;
		template <typename ...tfp_CComponent>
		bool f_Exists(tfp_CComponent const & ...p_Component) const;
		template <typename tf_CObject, typename ...tfp_CComponent>
		bool f_Read(tf_CObject &_Object, tfp_CComponent const & ...p_Component) const;
		template <typename tf_CObject, typename ...tfp_CComponent>
		void f_Write(tf_CObject const &_Object, tfp_CComponent const & ...p_Component) const;

		CDistributedActorTrustManager_Address f_DecodeAddress(CEJsonSorted const &_Json, CStr const &_Name) const;
		void f_EncodeAddress(CEJsonSorted &_Json, CDistributedActorTrustManager_Address const &_Address) const;

		CEJsonSorted f_ToJson(CSerial const &_Serial) const;
		void f_FromJson(CSerial &_Serial, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CBasicConfig const &_BasicConfig) const;
		void f_FromJson(CBasicConfig &_BasicConfig, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CInternalServerCertificate const &_Certificate) const;
		void f_FromJson(CServerCertificate &_ServerCertificate, CEJsonSorted const &_Json, CStr const &_Name) const;
		void f_FromJson(CInternalServerCertificate &_ServerCertificate, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CDefaultUser const &_DefaultUser) const;
		void f_FromJson(CDefaultUser &_DefaultUser, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CClient const &_Client) const;
		void f_FromJson(CClient &_Client, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CInternalClientConnection const &_ClientConnection) const;
		void f_FromJson(CClientConnection &_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const;
		void f_FromJson(CInternalClientConnection &_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CInternalListenConfig const &_ListenConfig) const;
		void f_FromJson(CListenConfig &_ListenConfig, CEJsonSorted const &_Json, CStr const &_Name) const;
		void f_FromJson(CInternalListenConfig &_ListenConfig, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CNamespace const &_Namespace) const;
		void f_FromJson(CNamespace &_Namespace, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CPermissions const &_Permissions) const;
		void f_FromJson(CPermissions &_Permissions, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CUserInfo const &_UserInfo) const;
		void f_FromJson(CUserInfo &_UserInfo, CEJsonSorted const &_Json, CStr const &_Name) const;
		CEJsonSorted f_ToJson(CUserAuthenticationFactor const &_Factor) const;
		void f_FromJson(CUserAuthenticationFactor &_Factor, CEJsonSorted const &_Json, CStr const &_Name) const;

		CEJsonSorted f_ToJson(CPrimaryListen const &_ClientConnection) const;
		void f_FromJson(CPrimaryListen &o_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const;

		void f_DoConversion(uint32 _OldVersion) const;

		CStr m_BaseDirectory;
	};

	struct CDistributedActorTrustManagerDatabase_JsonDirectory::CInternal : public CActorInternal
	{
		CInternal(CStr const &_BaseDirectory);

		TCWrapped<void> f_CheckState() const;

		NStorage::TCSharedPointer<CCanDestroyTracker> m_pCanDestroyTracker = fg_Construct();
		NStorage::TCSharedPointer<CJsonDirectoryStore const> m_pStore;
		CSequencer m_Sequencer{"Json Directory"};

		bool m_bGotBasicConfig = false;
	};

	CDistributedActorTrustManagerDatabase_JsonDirectory::CDistributedActorTrustManagerDatabase_JsonDirectory(CStr const &_BaseDirectory)
		: mp_pInternal(fg_Construct(_BaseDirectory))
	{
	}

	CDistributedActorTrustManagerDatabase_JsonDirectory::~CDistributedActorTrustManagerDatabase_JsonDirectory()
	{
		mp_pInternal.f_Clear();
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::fp_Destroy()
	{
		auto &Internal = *mp_pInternal;

		co_await fg_Move(Internal.m_Sequencer).f_Destroy().f_Wrap() > fg_LogError("Mib/Concurrency/JsonDirectory", "Failed to destroy sequencer");

		auto pCanDestroy = fg_Move(Internal.m_pCanDestroyTracker);
		auto CanDetroyFuture = pCanDestroy->f_Future();
		pCanDestroy.f_Clear();
		co_await fg_Move(CanDetroyFuture);

		co_return {};
	}

	TCWrapped<void> CDistributedActorTrustManagerDatabase_JsonDirectory::CInternal::f_CheckState() const
	{
		if (!m_bGotBasicConfig)
			return DMibErrorInstance("You have to call f_GetBasicConfig() before doing any other operations");

		return {};
	}

	void CJsonDirectoryStore::f_DoConversion(uint32 _OldVersion) const
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
		if (_OldVersion < 0x102)
		{
			CStr ServerCertificatesDirectory = m_BaseDirectory / "ServerCertificates";
			CStr ServerCertificatesDirectoryTemp = m_BaseDirectory / "ServerCertificatesTemp";
			CStr ServerCertificatesDirectoryDelete = m_BaseDirectory / "ServerCertificatesDelete";
			if (NFile::CFile::fs_FileExists(ServerCertificatesDirectoryTemp))
				NFile::CFile::fs_DeleteDirectoryRecursive(ServerCertificatesDirectoryTemp);
			if (NFile::CFile::fs_FileExists(ServerCertificatesDirectoryDelete))
				NFile::CFile::fs_DeleteDirectoryRecursive(ServerCertificatesDirectoryDelete);

			NContainer::TCMap<CStr, CInternalServerCertificate> Certificates;
			for (auto &FileName : f_Find("ServerCertificates"))
			{
				CStr HostName = f_PercentDecode(FileName);
				auto &Certificate = Certificates[HostName];
				Certificate.m_HostName = HostName;

				f_Read(Certificate.m_ServerCertificate, "ServerCertificates", FileName);
			}

			for (auto &Certificate : Certificates)
			{
				CStr FileName = f_GetNameHash(Certificates.fs_GetKey(Certificate));
				f_Write(Certificate, "ServerCertificatesTemp", FileName);
			}

			if (NFile::CFile::fs_FileExists(ServerCertificatesDirectory))
				NFile::CFile::fs_RenameFile(ServerCertificatesDirectory, ServerCertificatesDirectoryDelete);
			if (!Certificates.f_IsEmpty())
				NFile::CFile::fs_RenameFile(ServerCertificatesDirectoryTemp, ServerCertificatesDirectory);
			if (NFile::CFile::fs_FileExists(ServerCertificatesDirectoryDelete))
				NFile::CFile::fs_DeleteDirectoryRecursive(ServerCertificatesDirectoryDelete);
		}
	}

	TCFuture<CBasicConfig> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetBasicConfig()
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		auto BlockingActorCheckout = fg_BlockingActor();
		auto BasicConfig = co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;
					CBasicConfig BasicConfig;
					if (Store.f_Exists("BasicConfig"))
					{
						BasicConfig.m_Version = 0;
						Store.f_Read(BasicConfig, "BasicConfig");
					}

					if (BasicConfig.m_Version > CBasicConfig::mc_ConversionVersion)
						DMibError("Invalid conversion version 0x{nfh} > 0x{nfh}"_f << BasicConfig.m_Version << CBasicConfig::mc_ConversionVersion);
					else if (BasicConfig.m_Version < CBasicConfig::mc_ConversionVersion)
					{
						Store.f_DoConversion(BasicConfig.m_Version);
						BasicConfig.m_Version = CBasicConfig::mc_ConversionVersion;
						Store.f_Write(BasicConfig, "BasicConfig");
					}
					return BasicConfig;
				}
			)
		;

		Internal.m_bGotBasicConfig = true;

		co_return fg_Move(BasicConfig);
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetBasicConfig(CBasicConfig _BasicConfig)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _BasicConfig]
				{
					auto &Store = *pStore;
					Store.f_Write(_BasicConfig, "BasicConfig");
				}
			)
		;
	}

	TCFuture<int32> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetNewCertificateSerial()
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;
					CJsonDirectoryStore::CSerial Serial;
					Store.f_Read(Serial, "CertificateSerial");
					int32 ReturnSerial = Serial.m_Serial++;
					Store.f_Write(Serial, "CertificateSerial");
					return ReturnSerial;
				}
			)
		;
	}

	TCFuture<CDefaultUser> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetDefaultUser()
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;
					CDefaultUser DefaultUser;
					Store.f_Read(DefaultUser, "DefaultUser");
					return DefaultUser;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetDefaultUser(CDefaultUser _DefaultUser)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _DefaultUser]
				{
					auto &Store = *pStore;
					Store.f_Write(_DefaultUser, "DefaultUser");
				}
			)
		;
	}

	TCFuture<NContainer::TCMap<CStr, CServerCertificate>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumServerCertificates(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;

					NContainer::TCMap<CStr, CServerCertificate> ReturnCertificates;
					for (auto &FileName : Store.f_Find("ServerCertificates"))
					{
						CJsonDirectoryStore::CInternalServerCertificate InternalServerCertificate;
						Store.f_Read(InternalServerCertificate, "ServerCertificates", FileName);
						ReturnCertificates[InternalServerCertificate.m_HostName] = fg_Move(InternalServerCertificate.m_ServerCertificate);
					}

					return ReturnCertificates;
				}
			)
		;
	}

	TCFuture<CServerCertificate> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetServerCertificate(CStr _HostName)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostName]
				{
					auto &Store = *pStore;
					CServerCertificate ServerCertificate;
					CStr FileName = Store.f_GetNameHash(_HostName);
					if (!Store.f_Read(ServerCertificate, "ServerCertificates", FileName))
						DMibError("No server certificate for host");
					return ServerCertificate;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddServerCertificate(CStr _HostName, CServerCertificate _Certificate)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostName, _Certificate]
				{
					auto &Store = *pStore;
					CStr FileName = Store.f_GetNameHash(_HostName);
					if (Store.f_Exists("ServerCertificates", FileName))
						DMibError("Server certificate already exists");
					CJsonDirectoryStore::CInternalServerCertificate Certificate;
					Certificate.m_HostName = _HostName;
					Certificate.m_ServerCertificate = _Certificate;
					Store.f_Write(Certificate, "ServerCertificates", FileName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetServerCertificate(CStr _HostName, CServerCertificate _Certificate)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostName, _Certificate]
				{
					auto &Store = *pStore;
					CStr FileName = Store.f_GetNameHash(_HostName);
					if (!Store.f_Exists("ServerCertificates", FileName))
						DMibError("No server certificate for host");
					CJsonDirectoryStore::CInternalServerCertificate Certificate;
					Certificate.m_HostName = _HostName;
					Certificate.m_ServerCertificate = _Certificate;
					Store.f_Write(Certificate, "ServerCertificates", FileName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveServerCertificate(CStr _HostName)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostName]
				{
					auto &Store = *pStore;
					CStr FileName = Store.f_GetNameHash(_HostName);
					if (!Store.f_Delete("ServerCertificates", FileName))
						DMibError("No server certificate for host");
				}
			)
		;
	}

	auto CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumListenConfigs() -> TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CListenConfig>>
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;
					auto FileNames = Store.f_Find("ListenConfigs");

					NContainer::TCMap<CDistributedActorTrustManager_Address, CListenConfig> ListenConfigs;
					for (auto const &FileName : FileNames)
					{
						CJsonDirectoryStore::CInternalListenConfig ListenConfig;
						if (!Store.f_Read(ListenConfig, "ListenConfigs", FileName))
							DMibError("Internal error reading listen config");
						ListenConfigs[ListenConfig.m_Address] = ListenConfig.m_ListenConfig;
					}

					return ListenConfigs;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddListenConfig(CDistributedActorTrustManager_Address _Address, CListenConfig _Config)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address, _Config]
				{
					auto &Store = *pStore;
					auto NameHash = Store.f_GetNameHash(_Address);
					if (Store.f_Exists("ListenConfigs", NameHash))
						DMibError("Listen config already exists");
					CJsonDirectoryStore::CInternalListenConfig ListenConfig;
					ListenConfig.m_Address = _Address;
					ListenConfig.m_ListenConfig = _Config;
					Store.f_Write(ListenConfig, "ListenConfigs", NameHash);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetListenConfig(CDistributedActorTrustManager_Address _Address, CListenConfig _Config)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address, _Config]
				{
					auto &Store = *pStore;
					auto NameHash = Store.f_GetNameHash(_Address);
					if (!Store.f_Exists("ListenConfigs", NameHash))
						DMibError("No such listen config");
					CJsonDirectoryStore::CInternalListenConfig ListenConfig;
					ListenConfig.m_Address = _Address;
					ListenConfig.m_ListenConfig = _Config;
					Store.f_Write(ListenConfig, "ListenConfigs", NameHash);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveListenConfig(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address]
				{
					auto &Store = *pStore;

					// Clear the primary first; deleting the config first can make retries fail before repairing a dangling primary.
					CJsonDirectoryStore::CPrimaryListen PrimaryListen;
					if (Store.f_Read(PrimaryListen, "PrimaryListen") && PrimaryListen.m_Address == _Address)
						Store.f_Delete("PrimaryListen");

					auto NameHash = Store.f_GetNameHash(_Address);
					if (!Store.f_Delete("ListenConfigs", NameHash))
						DMibError("No such listen config");
				}
			)
		;
	}

	TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetPrimaryListen()
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;

					NStorage::TCOptional<CDistributedActorTrustManager_Address> Return;
					CJsonDirectoryStore::CPrimaryListen PrimaryListen;
					if (Store.f_Read(PrimaryListen, "PrimaryListen"))
						Return = PrimaryListen.m_Address;

					return Return;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address]
				{
					auto &Store = *pStore;

					if (_Address)
					{
						CJsonDirectoryStore::CPrimaryListen Listen;
						Listen.m_Address = *_Address;
						Store.f_Write(Listen, "PrimaryListen");
					}
					else
						Store.f_Delete("PrimaryListen");
				}
			)
		;
	}

	TCFuture<NContainer::TCMap<CStr, CClient>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumClients(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _bIncludeFullInfo]
				{
					auto &Store = *pStore;
					NContainer::TCMap<CStr, CClient> Clients;
					for (auto &HostID : Store.f_Find("Clients"))
					{
						auto &Client = Clients[HostID];
						if (_bIncludeFullInfo)
							Store.f_Read(Client, "Clients", HostID);
					}
					return Clients;
				}
			)
		;
	}

	TCFuture<CClient> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetClient(CStr _HostID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID]
				{
					auto &Store = *pStore;

					CClient Client;
					if (!Store.f_Read(Client, "Clients", _HostID))
						DMibError("No client for host ID");
					return Client;
				}
			)
		;
	}

	TCFuture<NStorage::TCUniquePointer<CClient>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_TryGetClient(CStr _HostID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID]() -> NStorage::TCUniquePointer<CClient>
				{
					auto &Store = *pStore;

					CClient Client;
					if (!Store.f_Read(Client, "Clients", _HostID))
						return nullptr;
					return fg_Construct(fg_Move(Client));
				}
			)
		;
	}

	TCFuture<bool> CDistributedActorTrustManagerDatabase_JsonDirectory::f_HasClient(CStr _HostID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID]
				{
					auto &Store = *pStore;
					return Store.f_Exists("Clients", _HostID);
				}
			)
		;
	}

	TCFuture<CStr> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetClientLastFriendlyName(NStr::CStr _HostID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID]() -> CStr
				{
					auto &Store = *pStore;

					CClient Client;
					if (!Store.f_Read(Client, "Clients", _HostID))
						return {};

					return Client.m_LastFriendlyName;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddClient(CStr _HostID, CClient _Client)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID, _Client]
				{
					auto &Store = *pStore;
					if (Store.f_Exists("Clients", _HostID))
						DMibError("Client already exists");
					Store.f_Write(_Client, "Clients", _HostID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetClient(CStr _HostID, CClient _Client)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID, _Client]
				{
					auto &Store = *pStore;
					if (!Store.f_Exists("Clients", _HostID))
						DMibError("No client for host ID");
					Store.f_Write(_Client, "Clients", _HostID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveClient(CStr _HostID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _HostID]
				{
					auto &Store = *pStore;
					if (!Store.f_Delete("Clients", _HostID))
						DMibError("No client for host ID");
				}
			)
		;
	}

	auto CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumClientConnections(bool _bIncludeFullInfo)
		-> TCFuture<NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection>>
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _bIncludeFullInfo]
				{
					auto &Store = *pStore;
					auto FileNames = Store.f_Find("ClientConnections");

					NContainer::TCMap<CDistributedActorTrustManager_Address, CClientConnection> ClientConnections;
					for (auto const &FileName : FileNames)
					{
						CJsonDirectoryStore::CInternalClientConnection ClientConnection;
						Store.f_Read(ClientConnection, "ClientConnections", FileName);
						if (_bIncludeFullInfo)
							ClientConnections[ClientConnection.m_Address] = ClientConnection.m_ClientConnection;
						else
							ClientConnections[ClientConnection.m_Address];
					}

					return ClientConnections;
				}
			)
		;
	}

	TCFuture<CClientConnection> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetClientConnection(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address]
				{
					auto &Store = *pStore;
					CClientConnection ClientConnection;
					if (!Store.f_Read(ClientConnection, "ClientConnections", Store.f_GetNameHash(_Address)))
						DMibError("No client connection for address");
					return ClientConnection;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddClientConnection(CDistributedActorTrustManager_Address _Address, CClientConnection _ClientConnection)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address, _ClientConnection]
				{
					auto &Store = *pStore;
					CStr NameHash = Store.f_GetNameHash(_Address);
					if (Store.f_Exists("ClientConnections", NameHash))
						DMibError("Client connection already exists");
					CJsonDirectoryStore::CInternalClientConnection ClientConnection;
					ClientConnection.m_Address = _Address;
					ClientConnection.m_ClientConnection = _ClientConnection;
					Store.f_Write(ClientConnection, "ClientConnections", NameHash);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetClientConnection(CDistributedActorTrustManager_Address _Address, CClientConnection _ClientConnection)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address, _ClientConnection]
				{
					auto &Store = *pStore;
					CEJsonSorted Json;

					CStr NameHash = Store.f_GetNameHash(_Address);
					if (!Store.f_Exists("ClientConnections", NameHash))
						DMibError("No client connection for address");
					CJsonDirectoryStore::CInternalClientConnection ClientConnection;
					ClientConnection.m_Address = _Address;
					ClientConnection.m_ClientConnection = _ClientConnection;
					Store.f_Write(ClientConnection, "ClientConnections", NameHash);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveClientConnection(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Address]
				{
					auto &Store = *pStore;
					if (!Store.f_Delete("ClientConnections", Store.f_GetNameHash(_Address)))
						DMibError("No client connection for address");
				}
			)
		;
	}

	TCFuture<NContainer::TCMap<CStr, CNamespace>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumNamespaces(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _bIncludeFullInfo]
				{
					auto &Store = *pStore;
					NContainer::TCMap<CStr, CNamespace> Namespaces;
					for (auto &NamespaceName : Store.f_Find("Namespaces"))
					{
						CStr DecodedNamespaceName = NamespaceName.f_ReplaceChar('_', '/');
						auto &Namespace = Namespaces[DecodedNamespaceName];
						if (_bIncludeFullInfo)
							Store.f_Read(Namespace, "Namespaces", NamespaceName);
					}
					return Namespaces;
				}
			)
		;
	}

	TCFuture<CNamespace> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetNamespace(CStr _NamespaceName)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _NamespaceName]
				{
					auto &Store = *pStore;

					if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
						DMibError("Invalid namespace name");
					CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

					CNamespace Namespace;
					if (!Store.f_Read(Namespace, "Namespaces", NamespaceName))
						DMibError("No namespace with that name");
					return Namespace;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddNamespace(CStr _NamespaceName, CNamespace _Namespace)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _NamespaceName, _Namespace]
				{
					if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
						DMibError("Invalid namespace name");
					CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

					auto &Store = *pStore;
					if (Store.f_Exists("Namespaces", NamespaceName))
						DMibError("Namespace already exists");
					Store.f_Write(_Namespace, "Namespaces", NamespaceName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetNamespace(CStr _NamespaceName, CNamespace _Namespace)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _NamespaceName, _Namespace]
				{
					if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
						DMibError("Invalid namespace name");
					CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

					auto &Store = *pStore;
					if (!Store.f_Exists("Namespaces", NamespaceName))
						DMibError("No namespace with that name");
					Store.f_Write(_Namespace, "Namespaces", NamespaceName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveNamespace(CStr _NamespaceName)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _NamespaceName]
				{
					if (!CActorDistributionManager::fs_IsValidNamespaceName(_NamespaceName))
						DMibError("Invalid namespace name");
					CStr NamespaceName = _NamespaceName.f_ReplaceChar('/', '_');

					auto &Store = *pStore;
					if (!Store.f_Delete("Namespaces", NamespaceName))
						DMibError("No namespace with that name");
				}
			)
		;
	}

	TCFuture<NContainer::TCMap<CPermissionIdentifiers, CPermissions>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumPermissions(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _bIncludeFullInfo]
				{
					auto &Store = *pStore;
					NContainer::TCMap<CPermissionIdentifiers, CPermissions> Permissions;
					for (auto &FileName : Store.f_Find("Permissions"))
					{
						CPermissionIdentifiers Key = CPermissionIdentifiers::fs_GetKeyFromFileName(FileName);
						auto &Permission = Permissions[Key];
						if (_bIncludeFullInfo)
							Store.f_Read(Permission, "Permissions", FileName);
					}
					return Permissions;
				}
			)
		;
	}

	TCFuture<CPermissions> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetPermissions(CPermissionIdentifiers _Identity)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Identity]
				{
					CStr FileName = _Identity.f_GetStr();
					auto &Store = *pStore;

					CPermissions Permissions;
					if (!Store.f_Read(Permissions, "Permissions", FileName))
						DMibError("No host permissions for that identity");
					return Permissions;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddPermissions(CPermissionIdentifiers _Identity, CPermissions _Permissions)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Identity, _Permissions]
				{
					CStr FileName = _Identity.f_GetStr();
					auto &Store = *pStore;
					if (Store.f_Exists("Permissions", FileName))
						DMibError("Host permissions already exists for that identity");
					Store.f_Write(_Permissions, "Permissions", FileName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetPermissions(CPermissionIdentifiers _Identity, CPermissions _Permissions)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Identity, _Permissions]
				{
					CStr FileName = _Identity.f_GetStr();
					auto &Store = *pStore;
					if (!Store.f_Exists("Permissions", FileName))
						DMibError("No host permissions for that identity");
					Store.f_Write(_Permissions, "Permissions", FileName);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemovePermissions(CPermissionIdentifiers _Identity)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _Identity]
				{
					CStr FileName = _Identity.f_GetStr();
					auto &Store = *pStore;
					if (!Store.f_Delete("Permissions", FileName))
						DMibError("No host permissions for that identity");
				}
			)
		;
	}

	TCFuture<NContainer::TCMap<CStr, CUserInfo>> CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumUsers(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker]
				{
					auto &Store = *pStore;
					NContainer::TCMap<CStr, CUserInfo> AllUsers;
					for (auto &UserID : Store.f_Find("Users"))
					{
						CUserInfo UserInfo;
						Store.f_Read(UserInfo, "Users", UserID);
						AllUsers[UserID] = UserInfo;
					}
					return AllUsers;
				}
			)
		;
	}

	TCFuture<CUserInfo> CDistributedActorTrustManagerDatabase_JsonDirectory::f_GetUserInfo(CStr _UserID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID]
				{
					auto &Store = *pStore;
					CUserInfo UserInfo;
					if (!Store.f_Read(UserInfo, "Users", _UserID))
						DMibError("No information found for that user ID");
					return UserInfo;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddUser(CStr _UserID, CUserInfo _UserInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID, _UserInfo]
				{
					auto &Store = *pStore;
					if (Store.f_Exists("Users", _UserID))
						DMibError("User info already exists for that user ID");
					Store.f_Write(_UserInfo, "Users", _UserID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetUserInfo(CStr _UserID, CUserInfo _UserInfo)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID, _UserInfo]
				{
					auto &Store = *pStore;
					if (!Store.f_Exists("Users", _UserID))
						DMibError("User '{}' does not exists"_f << _UserID);
					Store.f_Write(_UserInfo, "Users", _UserID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveUser(CStr _UserID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID]
				{
					auto &Store = *pStore;
					if (!Store.f_Delete("Users", _UserID))
						DMibError("No user info for that user ID");
				}
			)
		;
	}

	auto CDistributedActorTrustManagerDatabase_JsonDirectory::f_EnumAuthenticationFactor(bool _bIncludeFullInfo)
		-> TCFuture<NContainer::TCMap<CStr, NContainer::TCMap<CStr, CUserAuthenticationFactor>>>
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _bIncludeFullInfo]
				{
					auto &Store = *pStore;
					NContainer::TCMap<CStr, NContainer::TCMap<CStr, CUserAuthenticationFactor>> AllFactors;
					for (auto &FactorIDFactor : Store.f_FindRecursive("AuthenticationFactors"))
					{
						CUserAuthenticationFactor Factor;
						Store.f_Read(Factor, "AuthenticationFactors", FactorIDFactor);
						if (!_bIncludeFullInfo)
							Factor.m_PrivateData.f_Clear();
						AllFactors[NFile::CFile::fs_GetPath(FactorIDFactor)][NFile::CFile::fs_GetFile(FactorIDFactor)] = Factor;
					}
					return AllFactors;
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_AddUserAuthenticationFactor(CStr _UserID, CStr _FactorID, CUserAuthenticationFactor _Factor)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID, _FactorID, _Factor]
				{
					auto &Store = *pStore;
					if (!Store.f_Exists("Users", _UserID))
						DMibError("User '{}' does not exists"_f << _UserID);
					if (Store.f_Exists("AuthenticationFactors", _UserID, _FactorID))
						DMibError("User '{}' already has authentication factor ID '{}' registered"_f << _UserID << _FactorID);
					Store.f_Write(_Factor, "AuthenticationFactors", _UserID, _FactorID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_SetUserAuthenticationFactor(CStr _UserID, CStr _FactorID, CUserAuthenticationFactor _Factor)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID, _FactorID, _Factor]
				{
					auto &Store = *pStore;
					if (!Store.f_Exists("Users", _UserID))
						DMibError("User '{}' does not exists"_f << _UserID);
					if (!Store.f_Exists("AuthenticationFactors", _UserID, _FactorID))
						DMibError("User '{}' Does not have authentication factor ID '{}' registered"_f << _UserID << _FactorID);
					Store.f_Write(_Factor, "AuthenticationFactors", _UserID, _FactorID);
				}
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManagerDatabase_JsonDirectory::f_RemoveUserAuthenticationFactor(CStr _UserID, CStr _FactorID)
	{
		auto &Internal = *mp_pInternal;
		auto SequenceSubscription = co_await Internal.m_Sequencer.f_Sequence();
		co_await Internal.f_CheckState();
		auto BlockingActorCheckout = fg_BlockingActor();
		co_return co_await
			(
				g_Dispatch(BlockingActorCheckout) / [pStore = Internal.m_pStore, pCanDestroy = Internal.m_pCanDestroyTracker, _UserID, _FactorID]
				{
					auto &Store = *pStore;
					if (!Store.f_Delete("AuthenticationFactors", _UserID, _FactorID))
						DMibError("No authentication factor '{}' registered for user ID '{}'"_f << _FactorID << _UserID);
				}
			)
		;
	}

	CDistributedActorTrustManagerDatabase_JsonDirectory::CInternal::CInternal(CStr const &_BaseDirectory)
		: m_pStore(fg_Construct(_BaseDirectory))
	{
	}

	CJsonDirectoryStore::CJsonDirectoryStore(CStr const &_BaseDirectory)
		: m_BaseDirectory(_BaseDirectory)
	{
	}

	template <typename ...tfp_CComponent>
	CStr CJsonDirectoryStore::f_GetPath(tfp_CComponent const & ...p_Component) const
	{
		CStr Return = m_BaseDirectory;

		(
			[&]
			{
				Return += "/";
				Return += p_Component;
			}
			()
			, ...
		);

		Return += ".json";
		return Return;
	}

	template <typename ...tfp_CComponent>
	bool CJsonDirectoryStore::f_Delete(tfp_CComponent const & ...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);
		if (!NFile::CFile::fs_FileExists(Path))
			return false;
		NFile::CFile::fs_DeleteFile(Path);
		return true;
	}

	CStr CJsonDirectoryStore::f_GetNameHash(CDistributedActorTrustManager_Address const &_Object) const
	{
		NCryptography::TCBinaryStreamHash<NCryptography::CHash_SHA256> HashStream;
		_Object.m_URL.f_Feed(HashStream, 0x101);
		auto Digest = HashStream.f_GetDigest();
		return Digest.f_GetString();
	}

	CStr CJsonDirectoryStore::f_GetNameHash(CStr const &_Object) const
	{
		NCryptography::TCBinaryStreamHash<NCryptography::CHash_SHA256> HashStream;
		HashStream << _Object;
		auto Digest = HashStream.f_GetDigest();
		return Digest.f_GetString();
	}

	CStr CJsonDirectoryStore::f_PercentEncode(CStr const &_Str) const
	{
		CStr Encoded;
		NWeb::NHTTP::CURL::fs_PercentEncode(Encoded, _Str);
		return Encoded;
	}

	CStr CJsonDirectoryStore::f_PercentDecode(CStr const &_Str) const
	{
		CStr Decoded;
		NWeb::NHTTP::CURL::fs_PercentDecode(Decoded, _Str);
		return Decoded;
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CJsonDirectoryStore::f_Find(bool _bRecursive, tfp_CComponent const & ...p_Component) const
	{
		using namespace NFile;
		CStr Path = f_GetPath(p_Component..., "*");

		CFile::CFindFilesOptions Options(Path, _bRecursive);
		Options.m_AttribMask = EFileAttrib_File;
		auto Files = CFile::fs_FindFiles(Options);

		NContainer::TCSet<CStr> ReturnNames;
		CStr const SearchPath = CFile::fs_GetPath(Path);
		for (auto const &File : Files)
			ReturnNames[CFile::fs_AppendPath(CFile::fs_MakePathRelative(CFile::fs_GetPath(File.m_Path), SearchPath), CFile::fs_GetFileNoExt(File.m_Path))];
		return ReturnNames;
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CJsonDirectoryStore::f_Find(tfp_CComponent const & ...p_Component) const
	{
		return f_Find(false, p_Component...);
	}

	template <typename ...tfp_CComponent>
	NContainer::TCSet<CStr> CJsonDirectoryStore::f_FindRecursive(tfp_CComponent const & ...p_Component) const
	{
		return f_Find(true, p_Component...);
	}

	template <typename ...tfp_CComponent>
	bool CJsonDirectoryStore::f_Exists(tfp_CComponent const & ...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);
		return NFile::CFile::fs_FileExists(Path);
	}

	template <typename tf_CObject, typename ...tfp_CComponent>
	bool CJsonDirectoryStore::f_Read(tf_CObject &o_Object, tfp_CComponent const & ...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);
		if (!NFile::CFile::fs_FileExists(Path))
			return false;

		f_FromJson(o_Object, CEJsonSorted::fs_FromString(NFile::CFile::fs_ReadStringFromFile(Path)), Path);
		return true;
	}

	template <typename tf_CObject, typename ...tfp_CComponent>
	void CJsonDirectoryStore::f_Write(tf_CObject const &_Object, tfp_CComponent const & ...p_Component) const
	{
		CStr Path = f_GetPath(p_Component...);

		NFile::CFile::fs_CreateDirectory(NFile::CFile::fs_GetPath(Path));
		// Only allow the user to read and write these files as they contain private keys
		NFile::EFileAttrib Attributes = NFile::EFileAttrib_UnixAttributesValid | NFile::EFileAttrib_UserRead | NFile::EFileAttrib_UserWrite;
		CStr TempFile = Path + ".tmp";
		NFile::CFile::fs_WriteStringToFile(TempFile, f_ToJson(_Object).f_ToString(), true, Attributes);
		NFile::CFile::fs_AtomicReplaceFile(TempFile, Path);
	}

	CDistributedActorTrustManager_Address CJsonDirectoryStore::f_DecodeAddress(CEJsonSorted const &_Json, CStr const &_Name) const
	{
		CDistributedActorTrustManager_Address Address;
		Address.m_URL = _Json.f_String();

		CStr FileName = NFile::CFile::fs_GetFileNoExt(_Name);

		if (f_GetNameHash(Address) != FileName)
			DMibError(fg_Format("Address does not match the hashed file name '{}'. Expected '{}'", _Name, f_GetNameHash(Address)));

		return Address;
	}

	void CJsonDirectoryStore::f_EncodeAddress(CEJsonSorted &o_Json, CDistributedActorTrustManager_Address const &_Address) const
	{
		o_Json = _Address.m_URL.f_Encode();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CSerial const &_Serial) const
	{
		CEJsonSorted Json;
		Json["Serial"] = _Serial.m_Serial;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CSerial &o_Serial, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_Serial.m_Serial = _Json["Serial"].f_Integer();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CBasicConfig const &_BasicConfig) const
	{
		CEJsonSorted Json;
		Json["HostID"] = _BasicConfig.m_HostID;
		Json["CAPrivateKey"] = _BasicConfig.m_CAPrivateKey.f_ToInsecure();
		Json["CACertificate"] = _BasicConfig.m_CACertificate;
		Json["Version"] = _BasicConfig.m_Version;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CBasicConfig &o_BasicConfig, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_BasicConfig.m_HostID = _Json["HostID"].f_String();
		if (auto *pValue = _Json.f_GetMember("CAPrivateKey"))
			o_BasicConfig.m_CAPrivateKey = pValue->f_Binary().f_ToSecure();
		if (auto *pValue = _Json.f_GetMember("CACertificate"))
			o_BasicConfig.m_CACertificate = pValue->f_Binary();
		if (auto *pValue = _Json.f_GetMember("Version"))
			o_BasicConfig.m_Version = pValue->f_Integer();
		else
			o_BasicConfig.m_Version = 0;
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CDefaultUser const &_DefaultUser) const
	{
		CEJsonSorted Json;
		Json["UserID"] = _DefaultUser.m_UserID;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CDefaultUser &o_DefaultUser, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_DefaultUser.m_UserID = _Json["UserID"].f_String();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CInternalServerCertificate const &_Certificate) const
	{
		CEJsonSorted Json;
		Json["Host"] = _Certificate.m_HostName;
		Json["PrivateKey"] = _Certificate.m_ServerCertificate.m_PrivateKey.f_ToInsecure();
		Json["PublicCertificate"] = _Certificate.m_ServerCertificate.m_PublicCertificate;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CInternalServerCertificate &o_ServerCertificate, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ServerCertificate.m_HostName = _Json["Host"].f_String();
		f_FromJson(o_ServerCertificate.m_ServerCertificate, _Json, _Name);
	}

	void CJsonDirectoryStore::f_FromJson(CServerCertificate &o_ServerCertificate, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ServerCertificate.m_PrivateKey = _Json["PrivateKey"].f_Binary().f_ToSecure();
		o_ServerCertificate.m_PublicCertificate = _Json["PublicCertificate"].f_Binary();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CClient const &_Client) const
	{
		CEJsonSorted Json;
		Json["PublicCertificate"] = _Client.m_PublicCertificate;
		Json["LastFriendlyName"] = _Client.m_LastFriendlyName;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CClient &o_Client, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_Client.m_PublicCertificate = _Json["PublicCertificate"].f_Binary();
		if (auto *pName = _Json.f_GetMember("LastFriendlyName"))
			o_Client.m_LastFriendlyName = pName->f_String();
		else
			o_Client.m_LastFriendlyName.f_Clear();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CInternalClientConnection const &_ClientConnection) const
	{
		CEJsonSorted Json;
		f_EncodeAddress(Json["Address"], _ClientConnection.m_Address);
		Json["PublicServerCertificate"] = _ClientConnection.m_ClientConnection.m_PublicServerCertificate;
		Json["PublicClientCertificate"] = _ClientConnection.m_ClientConnection.m_PublicClientCertificate;
		Json["LastFriendlyName"] = _ClientConnection.m_ClientConnection.m_LastFriendlyName;
		Json["ConnectionConcurrency"] = _ClientConnection.m_ClientConnection.m_ConnectionConcurrency;
		Json["SendWindowBytes"] = (int64)_ClientConnection.m_ClientConnection.m_SendWindowBytes;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CClientConnection &o_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ClientConnection.m_PublicServerCertificate = _Json["PublicServerCertificate"].f_Binary();
		o_ClientConnection.m_PublicClientCertificate = _Json["PublicClientCertificate"].f_Binary();
		if (auto *pName = _Json.f_GetMember("LastFriendlyName"))
			o_ClientConnection.m_LastFriendlyName = pName->f_String();
		else
			o_ClientConnection.m_LastFriendlyName.f_Clear();
		if (auto *pName = _Json.f_GetMember("ConnectionConcurrency"))
			o_ClientConnection.m_ConnectionConcurrency = pName->f_Integer();
		else
			o_ClientConnection.m_ConnectionConcurrency = -1;
		if (auto *pValue = _Json.f_GetMember("SendWindowBytes"))
			o_ClientConnection.m_SendWindowBytes = (uint64)pValue->f_Integer();
		else
			o_ClientConnection.m_SendWindowBytes = 0;
	}

	void CJsonDirectoryStore::f_FromJson(CInternalClientConnection &o_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ClientConnection.m_Address = f_DecodeAddress(_Json["Address"], _Name);
		f_FromJson(o_ClientConnection.m_ClientConnection, _Json, _Name);
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CPrimaryListen const &_ClientConnection) const
	{
		CEJsonSorted Json;
		f_EncodeAddress(Json["Address"], _ClientConnection.m_Address);
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CPrimaryListen &o_ClientConnection, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ClientConnection.m_Address.m_URL = _Json["Address"].f_String();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CInternalListenConfig const &_ListenConfig) const
	{
		CEJsonSorted Json;
		f_EncodeAddress(Json["Address"], _ListenConfig.m_Address);
		Json["SendWindowBytes"] = (int64)_ListenConfig.m_ListenConfig.m_SendWindowBytes;
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CListenConfig &o_ListenConfig, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		if (auto *pValue = _Json.f_GetMember("SendWindowBytes"))
			o_ListenConfig.m_SendWindowBytes = (uint64)pValue->f_Integer();
		else
			o_ListenConfig.m_SendWindowBytes = 0;
	}

	void CJsonDirectoryStore::f_FromJson(CInternalListenConfig &o_ListenConfig, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_ListenConfig.m_Address = f_DecodeAddress(_Json["Address"], _Name);
		f_FromJson(o_ListenConfig.m_ListenConfig, _Json, _Name);
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CNamespace const &_Namespace) const
	{
		CEJsonSorted Json;
		auto &AllowedHosts = Json["AllowedHosts"].f_Array();
		for (auto &AllowedHost : _Namespace.m_AllowedHosts)
			AllowedHosts.f_Insert(AllowedHost);
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CNamespace &o_Namespace, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_Namespace.m_AllowedHosts.f_Clear();
		for (auto &AllowedHost : _Json["AllowedHosts"].f_Array())
			o_Namespace.m_AllowedHosts[AllowedHost.f_String()];
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CPermissions const &_Permissions) const
	{
		CEJsonSorted Json;
		auto &PermissionsObject = Json["Permissions"] = EJsonType_Object;
		for (auto &AuthenticationFactors : _Permissions.m_Permissions)
		{
			auto &RequiredFactors = PermissionsObject[_Permissions.m_Permissions.fs_GetKey(AuthenticationFactors)] = EJsonType_Object;
			RequiredFactors["MaximumAuthenticationLifetime"] = AuthenticationFactors.m_MaximumAuthenticationLifetime;
			auto &Factors = RequiredFactors["RequiredAuthenticationFactors"].f_Array();
			for (auto const &InnerSet : AuthenticationFactors.m_AuthenticationFactors)
			{
				CEJsonSorted Array = EJsonType_Array;
				for (auto const &Factor : InnerSet)
					Array.f_Insert(Factor);
				Factors.f_Insert(Array);
			}
		}
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CPermissions &o_Permissions, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_Permissions.m_Permissions.f_Clear();
		auto &PermissionsObject = _Json["Permissions"];
		if (PermissionsObject.f_IsObject())
		{
			for (auto &Permission : PermissionsObject.f_Object())
			{
				CPermissionRequirements AuthenticationFactors;
				auto &FactorsObject = Permission.f_Value().f_Object();
				if (auto pValue = FactorsObject.f_GetMember("MaximumAuthenticationLifetime"))
					AuthenticationFactors.m_MaximumAuthenticationLifetime = pValue->f_Integer();

				auto pValue = FactorsObject.f_GetMember("RequiredAuthenticationFactors");
				for (auto &Inner : pValue->f_Array())
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

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CUserInfo const &_UserInfo) const
	{
		CEJsonSorted Json;
		Json["UserName"] = _UserInfo.m_UserName;
		if (!_UserInfo.m_Metadata.f_IsEmpty())
		{
			auto &Metadata = Json["Metadata"] = EJsonType_Object;
			for (auto &Item : _UserInfo.m_Metadata)
				Metadata[_UserInfo.m_Metadata.fs_GetKey(Item)] = Item;
		}
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CUserInfo &o_UserInfo, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_UserInfo.m_UserName = _Json["UserName"].f_String();
		o_UserInfo.m_Metadata.f_Clear();
		auto pValue = _Json.f_GetMember("Metadata");
		if (!pValue)
			return;

		auto Object = pValue->f_Object();
		for (auto iMetadata = Object.f_OrderedIterator(); iMetadata; ++iMetadata)
			o_UserInfo.m_Metadata[iMetadata->f_Name()] = iMetadata->f_Value();
	}

	CEJsonSorted CJsonDirectoryStore::f_ToJson(CUserAuthenticationFactor const &_Factor) const
	{
		CEJsonSorted Json;
		Json["Category"] = _Factor.m_Category;
		Json["Name"] = _Factor.m_Name;
		if (!_Factor.m_PublicData.f_IsEmpty())
		{
			auto &PublicData = Json["PublicData"] = EJsonType_Object;
			for (auto &Item : _Factor.m_PublicData)
				PublicData[_Factor.m_PublicData.fs_GetKey(Item)] = Item;
		}
		if (!_Factor.m_PrivateData.f_IsEmpty())
		{
			auto &PrivateData = Json["PrivateData"] = EJsonType_Object;
			for (auto &Item : _Factor.m_PrivateData)
				PrivateData[_Factor.m_PrivateData.fs_GetKey(Item)] = Item;
		}
		return Json;
	}

	void CJsonDirectoryStore::f_FromJson(CUserAuthenticationFactor &o_Factor, CEJsonSorted const &_Json, CStr const &_Name) const
	{
		o_Factor.m_Category = (EAuthenticationFactorCategory)_Json["Category"].f_Integer();
		o_Factor.m_Name = _Json["Name"].f_String();
		o_Factor.m_PublicData.f_Clear();
		o_Factor.m_PrivateData.f_Clear();

		if (auto pValue = _Json.f_GetMember("PublicData"))
		{
			auto Object = pValue->f_Object();
			for (auto iPublicData = Object.f_OrderedIterator(); iPublicData; ++iPublicData)
				o_Factor.m_PublicData[iPublicData->f_Name()] = iPublicData->f_Value();
		}

		if (auto pValue = _Json.f_GetMember("PrivateData"))
		{
			auto Object = pValue->f_Object();
			for (auto iPrivateData = Object.f_OrderedIterator(); iPrivateData; ++iPrivateData)
				o_Factor.m_PrivateData[iPrivateData->f_Name()] = iPrivateData->f_Value();
		}
	}
}
