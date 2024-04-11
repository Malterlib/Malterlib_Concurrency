// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"
#include "Malterlib_Concurrency_DistributedTrust_Proxy.h"

#include <Mib/String/String>
#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;

	TCFuture<NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManager::f_EnumUsers(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> Return;
		for (auto const &Source : Internal.m_Users)
		{
			auto &Destination = Return[Internal.m_Users.fs_GetKey(Source)];
			Destination.m_UserName = Source.m_UserInfo.m_UserName;
			Destination.m_Metadata = Source.m_UserInfo.m_Metadata;
		}
		co_return fg_Move(Return);
	}

	TCFuture<TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManager::f_TryGetUser(CStr const &_UserID)
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (auto *pUser = Internal.m_Users.f_FindEqual(_UserID))
			co_return CDistributedActorTrustManagerInterface::CUserInfo{pUser->m_UserInfo.m_UserName, pUser->m_UserInfo.m_Metadata};
		else
			co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_AddUser(CStr const &_UserID, CStr const &_UserName)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto &Users = Internal.m_Users;

		if (Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' already exists"_f << _UserID);

		auto &UserState = Internal.m_Users[_UserID];
		UserState.m_UserInfo = CUserInfo{_UserName};
		if (UserState.m_bExistsInDatabase)
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo);
		else
		{
			UserState.m_bExistsInDatabase = true;
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo);
		}

		co_return {};
	}
	
	TCFuture<void> CDistributedActorTrustManager::f_RemoveUser(NStr::CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			co_return DMibErrorInstance("No user with ID '{}'"_f << _UserID);

		bool bExistsInDatabase = pUser->m_bExistsInDatabase;

		TCActorResultVector<void> Results;
		if (auto *pFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			TCSet<CStr> Factors;
			for (auto &Factor : *pFactors)
			{
				if (Factor.m_bExistsInDatabase)
					Factors[pFactors->fs_GetKey(Factor)];
			}
			for (auto &Factor : Factors)
				self(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, _UserID, Factor) > Results.f_AddResult();

			Internal.m_UserAuthenticationFactors.f_Remove(_UserID);
		}

		if (bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveUser, _UserID) > Results.f_AddResult();
		Internal.m_Users.f_Remove(_UserID);

		co_await (co_await Results.f_GetResults() | g_Unwrap);

		co_return {};
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetUserInfo
		(
			CStr const &_UserID
			, TCOptional<CStr> const &_UserName
			, TCSet<CStr> const &_RemoveMetadata
			, TCMap<CStr, CEJSONSorted> const &_AddMetadata
		)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto &UserState = *pUser;

		// Make a copy so we don't accidentally overwrite the database
		auto UserInfo = UserState.m_UserInfo;

		if (_UserName)
		{
			if (_UserName->f_IsEmpty())
				co_return DMibErrorInstance("User name cannot be empty");

			UserInfo.m_UserName = *_UserName;
		}

		for (auto const &Key : _RemoveMetadata)
		{
			auto pValue = UserInfo.m_Metadata.f_FindEqual(Key);
			if (!pValue)
				co_return DMibErrorInstance("Key '{}' does not exist"_f << Key);

			UserInfo.m_Metadata.f_Remove(pValue);
		}

		for (auto const &Metadata : _AddMetadata)
		{
			auto const &Key = _AddMetadata.fs_GetKey(Metadata);
			if (!Metadata.f_IsValid())
				co_return DMibErrorInstance("Invalid metadata for key '{}'"_f << Key);

			UserInfo.m_Metadata[Key] = Metadata;
		}

		UserState.m_UserInfo = fg_Move(UserInfo);

		if (UserState.m_bExistsInDatabase)
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo);
		else
		{
			UserState.m_bExistsInDatabase = true;
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo);
		}

		co_return {};
	}
	
	TCFuture<NStr::CStr> CDistributedActorTrustManager::f_GetDefaultUser()
	{
		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		co_return Internal.m_DefaultUser.m_UserID;
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetDefaultUser(NStr::CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			co_return DMibErrorInstance("No user with ID '{}'"_f << _UserID);

		Internal.m_DefaultUser.m_UserID = _UserID;
		co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetDefaultUser, CDefaultUser{_UserID});

		co_return {};
	}

	template<typename t_CType>
	TCFuture<CStr> DMibWorkaroundUBSanSectionErrors fg_ExportUser(TCActor<t_CType> _TrustManager, CStr _UserID, bool _bIncludePrivate)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto UserInfo = co_await _TrustManager.template f_CallActor(&t_CType::f_TryGetUser)(_UserID);

		if (!UserInfo)
			co_return DMibErrorInstance("User {} does not exist"_f << _UserID);

		NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CLocalAuthenticationData> Factors
			= co_await _TrustManager.template f_CallActor(&t_CType::f_EnumUserAuthenticationFactors)(_UserID)
		;

		{
			auto CaptureScope = co_await g_CaptureExceptions;

			NStream::CBinaryStreamMemory<> Stream;
			uint32 Version = CDistributedActorTrustManagerInterface::EProtocolVersion_Current;
			DMibBinaryStreamVersion(Stream, Version);
			Stream << Version;
			Stream << _UserID;
			Stream << *UserInfo;

			if (!_bIncludePrivate)
			{
				for (auto &Factor : Factors)
					Factor.m_PrivateData.f_Clear();
			}

			Stream << Factors;

			co_return NEncoding::fg_Base64Encode(Stream.f_GetVector());
		}

		co_return {};
	}

	template<typename t_CType>
	TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrors fg_ImportUser(TCActor<t_CType> _TrustManager, NStr::CStr _UserData)
	{
		struct CResult
		{
			NStr::CStr m_UserID;
			CDistributedActorTrustManagerInterface::CUserInfo m_UserInfo;
			NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CLocalAuthenticationData> m_AuthenticationData;
		};

		CResult Result;

		{
			auto CaptureScope = co_await g_CaptureExceptions;

			NContainer::CByteVector Data;

			NEncoding::fg_Base64Decode(_UserData, Data);
			NStream::CBinaryStreamMemoryPtr<> Stream;
			Stream.f_OpenRead(Data);

			uint32 Version;
			Stream >> Version;
			if (Version > CDistributedActorTrustManagerInterface::EProtocolVersion_Current)
				DMibError("Unsupported export version 0x{nfh}"_f << Version);

			DMibBinaryStreamVersion(Stream, Version);

			Stream >> Result.m_UserID;

			if (!CActorDistributionManager::fs_IsValidUserID(Result.m_UserID))
				DMibError("Invalid user ID");

			Stream >> Result.m_UserInfo;
			if (!Result.m_UserInfo.m_UserName)
				DMibError("User name cannot be empty");

			for (auto const &Metadata : Result.m_UserInfo.m_Metadata)
			{
				if (!Metadata.f_IsValid())
					DMibError("Invalid metadata for key '{}'"_f << Result.m_UserInfo.m_Metadata.fs_GetKey(Metadata));
			}

			Stream >> Result.m_AuthenticationData;

			for (auto &Factor : Result.m_AuthenticationData)
			{
				auto &FactorName = Result.m_AuthenticationData.fs_GetKey(Factor);

				for (auto const &Data : Factor.m_PrivateData)
				{
					if (!Data.f_IsValid())
						DMibError("Invalid private data for key '{}' in factor '{}'"_f << Factor.m_PrivateData.fs_GetKey(Data) << FactorName);
				}

				for (auto const &Data : Factor.m_PublicData)
				{
					if (!Data.f_IsValid())
						DMibError("Invalid public data for key '{}' in factor '{}'"_f << Factor.m_PublicData.fs_GetKey(Data) << FactorName);
				}
			}
		}

		NStr::CStr UserID = Result.m_UserID;
		auto UserInfo = co_await _TrustManager.template f_CallActor(&t_CType::f_TryGetUser)(UserID);

		if (!UserInfo)
			co_await _TrustManager.template f_CallActor(&t_CType::f_AddUser)(Result.m_UserID, Result.m_UserInfo.m_UserName);

		co_await _TrustManager.template f_CallActor(&t_CType::f_SetUserInfo)(Result.m_UserID, Result.m_UserInfo.m_UserName, NContainer::TCSet<CStr>{}, Result.m_UserInfo.m_Metadata);
		auto Factors = co_await _TrustManager.template f_CallActor(&t_CType::f_EnumUserAuthenticationFactors)(Result.m_UserID);

		TCActorResultVector<void> Results;

		for (auto const &Data : Result.m_AuthenticationData)
		{
			auto FactorID = Result.m_AuthenticationData.fs_GetKey(Data);
			auto *pFactor = Factors.f_FindEqual(FactorID);

			if (pFactor)
			{
				if (pFactor->m_PrivateData.f_IsEmpty() || !Data.m_PrivateData.f_IsEmpty())
					_TrustManager.template f_CallActor(&t_CType::f_SetUserAuthenticationFactor)(Result.m_UserID, FactorID, fg_TempCopy(Data)) > Results.f_AddResult();
			}
			else
				_TrustManager.template f_CallActor(&t_CType::f_AddUserAuthenticationFactor)(Result.m_UserID, FactorID, fg_TempCopy(Data)) > Results.f_AddResult();
		}

#ifdef DCompiler_MSVC_Workaround
		auto Temp = co_await Results.f_GetResults();
		co_await (fg_Move(Temp) | g_Unwrap);
#else
		co_await (co_await Results.f_GetResults() | g_Unwrap);
#endif
		co_return Result.m_UserID;
	}

	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ImportUser(TCActor<CDistributedActorTrustManager> _TrustManager, NStr::CStr _UserData);
	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ImportUser(TCActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserData);
	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ImportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserData);
	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ExportUser(TCActor<CDistributedActorTrustManager> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ExportUser(TCActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
	template TCFuture<NStr::CStr> DMibWorkaroundUBSanSectionErrorsDisable fg_ExportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> _TrustManager, NStr::CStr _UserID, bool _bIncludePrivate);
}
