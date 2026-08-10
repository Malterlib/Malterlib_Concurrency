// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Cryptography/Certificate>

namespace NMib::NConcurrency
{
	namespace
	{
		// Stored listen addresses always carry a lower case scheme; the writing APIs reject other
		// spellings rather than normalizing
		bool fg_HasLowerCaseScheme(CDistributedActorTrustManager_Address const &_Address)
		{
			auto const &Scheme = _Address.m_URL.f_GetScheme();
			return Scheme == Scheme.f_LowerCase();
		}
	}

	TCFuture<CActorDistributionListenSettings> CDistributedActorTrustManager::f_GetCertificateData(CDistributedActorTrustManager_Address _Address) const
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto fDoReturn = [&](CServerCertificate const *_pServerCert)
			{
				auto &Internal = *mp_pInternal;

				CActorDistributionListenSettings ListenSettings(NContainer::fg_CreateVector<NWeb::NHTTP::CURL>(_Address.m_URL));
				ListenSettings.m_PrivateKey = _pServerCert->m_PrivateKey;
				ListenSettings.m_CACertificate = Internal.m_BasicConfig.m_CACertificate;
				ListenSettings.m_PublicCertificate = _pServerCert->m_PublicCertificate;
				ListenSettings.m_KeySetting = Internal.m_KeySetting;
				ListenSettings.m_bRetryOnListenFailure = false;
				ListenSettings.m_ListenFlags = Internal.m_ListenFlags;

				return ListenSettings;
			}
		;

		auto Host = _Address.m_URL.f_GetHost();

		auto *pServerCert = Internal.m_ServerCertificates.f_FindEqual(Host);
		if (pServerCert)
			co_return fDoReturn(pServerCert);

		auto Serial = co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_GetNewCertificateSerial) % "Failed to get new certificate serial");

		// Because of performance of generating a new key, run on separate actor
		auto ServerCert = co_await
			(
				fg_ConcurrentDispatch
				(
					[
						CaCertificate = Internal.m_BasicConfig.m_CACertificate
						, CaPrivateKey = Internal.m_BasicConfig.m_CAPrivateKey
						, KeySetting = Internal.m_KeySetting
						, Host
						, HostID = Internal.m_BasicConfig.m_HostID
						, Serial
					]
					() -> TCFuture<ICDistributedActorTrustManagerDatabase::CServerCertificate>
					{
						NCryptography::CCertificateOptions Options;
						Options.m_KeySetting = KeySetting;
						Options.m_CommonName = fg_Format("Malterlib Distributed Actors Listen - {}", Host).f_Left(64);
						Options.m_Hostnames.f_Insert(Host);
						auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
						Extension.m_bCritical = false;
						Extension.m_Value = HostID;

						NContainer::CByteVector CertificateRequest;
						ICDistributedActorTrustManagerDatabase::CServerCertificate ServerCert;
						try
						{
							NCryptography::CCertificate::fs_GenerateClientCertificateRequest
								(
									Options
									, CertificateRequest
									, ServerCert.m_PrivateKey
								)
							;
						}
						catch (NException::CException const &_Exception)
						{
							co_return DMibErrorInstance(fg_Format("Failed to generate listen certificate request: {}", _Exception.f_GetErrorStr()));
						}

						try
						{
							NCryptography::CCertificateSignOptions SignOptions;
							SignOptions.m_Serial = Serial;
							SignOptions.m_Days = 10*365;

							// The signer sets the subject rather than trusting the request's
							SignOptions.m_OverrideSubjectCommonName = fg_Format("Malterlib Distributed Actors Listen - {}", Host).f_Left(64);

							// This is the listen (server) leaf, the server identity presented on every
							// transport. None of the distributed-actor transports verify a hostname, so
							// the certificate role is the only thing distinguishing a server credential
							// from a client one: constrain it to a non-CA serverAuth leaf so a client leaf
							// issued by the same CA cannot be presented as this server
							SignOptions.m_LeafRole = NCryptography::ECertificateLeafRole_ServerAuth;

							SignOptions.m_AllowedKeyTypes.f_Insert(KeySetting);
							SignOptions.m_AllowedRequestDigests.f_Insert(NCryptography::fg_GetAutomaticDigestType(KeySetting));

							// The listen request is generated locally, but still restrict the copied
							// request extensions to the identity (MalterlibHostID) and the subjectAltName
							// it sets so the signer never propagates anything unexpected
							SignOptions.m_AllowedRequestExtensions.f_Insert("1.3.6.1.4.1.47722.1.1");
							SignOptions.m_AllowedRequestExtensions.f_Insert("2.5.29.17");

							NCryptography::CCertificate::fs_SignClientCertificate
								(
									CaCertificate
									, CaPrivateKey
									, CertificateRequest
									, ServerCert.m_PublicCertificate
									, SignOptions
								)
							;
						}
						catch (NException::CException const &_Exception)
						{
							co_return DMibErrorInstance(fg_Format("Failed to generate sign certificate request: {}", _Exception.f_GetErrorStr()));
						}

						co_return fg_Move(ServerCert);
					}
				)
			)
		;

		co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddServerCertificate, Host, ServerCert) % "Failed to save new server certificate to database");

		auto &CertOutput = Internal.m_ServerCertificates[Host] = fg_Move(ServerCert);

		co_return fDoReturn(&CertOutput);
	}

	TCFuture<void> CDistributedActorTrustManager::f_AddListen(CDistributedActorTrustManager_Address _Address)
	{
		auto CheckDestroy = co_await f_CheckDestroyedOnResume();

		auto &Internal = *mp_pInternal;

		if (!fg_HasLowerCaseScheme(_Address))
			co_return DMibErrorInstance("Listen address scheme must be lower case");

		// Validate the host the listen will actually bind: a logical alias that hostname translation
		// maps to a unix socket is a valid wsa listen even though the raw host is not
		if (auto Error = fg_ValidateAuthenticatedUnixAddress(_Address.m_URL.f_GetScheme(), Internal.f_TranslateHostname(_Address.m_URL.f_GetHost())); !Error.f_IsEmpty())
			co_return DMibErrorInstance(Error);

		co_await Internal.f_WaitForInit();

		CListenConfig ListenConfig;
		ListenConfig.m_Address = _Address;

		auto pOld = Internal.m_Listen.f_FindEqual(ListenConfig);
		if (pOld)
			co_return DMibErrorInstance("Already listening to address");

		auto ListenSettings = co_await f_GetCertificateData(_Address);

		auto ListenReference = co_await (Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Listen, fg_Move(ListenSettings)) % "Failed to listen");

		auto &ListenState = Internal.m_Listen[ListenConfig];
		ListenState.m_ListenReference = fg_Move(ListenReference);

		auto ConfigAddResult = co_await
			(
				Internal.m_Database.f_Bind<&ICDistributedActorTrustManagerDatabase::f_AddListenConfig>(ListenConfig)
				% "Failed to save new listen config to database"
			).f_Wrap()
		;

		if (!ConfigAddResult)
		{
			Internal.m_Listen.f_Remove(ListenConfig);
			co_return ConfigAddResult.f_GetException();
		}

		co_return {};
	}

	TCFuture<bool> CDistributedActorTrustManager::f_HasListen(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		ICDistributedActorTrustManagerDatabase::CListenConfig ListenConfig;
		ListenConfig.m_Address = _Address;

		auto *pListenConfig = Internal.m_Listen.f_FindEqual(ListenConfig);

		co_return pListenConfig != nullptr;
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetPrimaryListen(NStorage::TCOptional<CDistributedActorTrustManager_Address> _Address)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		// A concurrent f_RemoveListen keeps this pointer valid by clearing it in the same
		// synchronous step that erases the node; its database write is queued after this one
		if (_Address)
		{
			if (!fg_HasLowerCaseScheme(*_Address))
				co_return DMibErrorInstance("Listen address scheme must be lower case");

			ICDistributedActorTrustManagerDatabase::CListenConfig ListenConfig;
			ListenConfig.m_Address = *_Address;

			auto *pListen = Internal.m_Listen.f_FindEqual(ListenConfig);
			if (!pListen)
				co_return DMibErrorInstance("Listen address not found");

			Internal.m_pPrimaryListen = pListen;
		}
		else
			Internal.m_pPrimaryListen = nullptr;

		co_await (Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetPrimaryListen, _Address) % "Failed to set primary listen in database");

		co_return {};
	}

	TCFuture<NStorage::TCOptional<CDistributedActorTrustManager_Address>> CDistributedActorTrustManager::f_GetPrimaryListen()
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (Internal.m_pPrimaryListen)
			co_return Internal.m_Listen.fs_GetKey(*Internal.m_pPrimaryListen).m_Address;

		co_return {};
	}

	TCFuture<NContainer::TCSet<CDistributedActorTrustManager_Address>> CDistributedActorTrustManager::f_EnumListens()
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		NContainer::TCSet<CDistributedActorTrustManager_Address> Addresses;
		for (auto iClientConnection = Internal.m_Listen.f_GetIterator(); iClientConnection; ++iClientConnection)
			Addresses[iClientConnection.f_GetKey().m_Address];

		co_return fg_Move(Addresses);
	}

	TCFuture<void> CDistributedActorTrustManager::f_RemoveListen(CDistributedActorTrustManager_Address _Address)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!fg_HasLowerCaseScheme(_Address))
			co_return DMibErrorInstance("Listen address scheme must be lower case");

		ICDistributedActorTrustManagerDatabase::CListenConfig ListenConfig;
		ListenConfig.m_Address = _Address;

		// Remove the entry before the first suspension so a concurrent f_SetPrimaryListen for this
		// address fails up front. The extracted reference still stops the listen if the database
		// write below fails (its destructor stops fire and forget)
		CDistributedActorListenReference ListenReference;
		if (auto *pListen = Internal.m_Listen.f_FindEqual(ListenConfig))
		{
			ListenReference = fg_Move(pListen->m_ListenReference);
			if (pListen == Internal.m_pPrimaryListen)
				Internal.m_pPrimaryListen = nullptr;

			Internal.m_Listen.f_Remove(pListen);
		}

		// By design: the socket stays bound until f_Stop completes, so a racing f_AddListen must
		// retry. f_RemoveListenConfig also clears the persisted primary
		auto RemoveDatabaseFuture = Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveListenConfig, ListenConfig);

		if (ListenReference.f_IsActive())
			co_await ((fg_Move(RemoveDatabaseFuture) % "Failed to remove listen config from database") + (ListenReference.f_Stop() % "Failed to stop listen"));
		else
			co_await (fg_Move(RemoveDatabaseFuture) % "Failed to remove listen config from database");

		co_return {};
	}

	TCFuture<NStr::CStr> CDistributedActorTrustManager::CInternal::f_ValidateClientAccess
		(
			NStr::CStr _HostID
			, NContainer::TCVector<NContainer::CByteVector> _CertificateChain
		)
	{
		if (_CertificateChain.f_IsEmpty())
			co_return DMibErrorInstance("Empty certificate chain");

		co_await f_WaitForInit();

		auto Client = co_await (m_Database(&ICDistributedActorTrustManagerDatabase::f_GetClient, _HostID) % "Could not find client host");

		if (_CertificateChain.f_GetFirst() != Client.m_PublicCertificate)
			co_return NStr::gc_Str<"Access denied">.m_Str;

		co_return {};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
