// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

#include <Mib/String/String>
#include <Mib/Encoding/JSONShortcuts>

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NStorage;
	using namespace NEncoding;
	using namespace NContainer;

	TCContinuation<NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManager::f_EnumUsers(bool _bIncludeFullInfo)
	{
		auto &Internal = *mp_pInternal;
		NContainer::TCMap<NStr::CStr, CDistributedActorTrustManagerInterface::CUserInfo> Return;
		for (auto const &Source : Internal.m_Users)
		{
			auto &Destination = Return[Internal.m_Users.fs_GetKey(Source)];
			Destination.m_UserName = Source.m_UserInfo.m_UserName;
			Destination.m_Metadata = Source.m_UserInfo.m_Metadata;
		}
		return fg_Explicit(fg_Move(Return));
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_AddUser(CStr const &_UserID, CStr const &_UserName)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		auto &Users = Internal.m_Users;

		if (Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' already exists"_f << _UserID);

		TCContinuation<void> SaveDatabaseContinuation;
		auto &UserState = Internal.m_Users[_UserID];
		UserState.m_UserInfo = CUserInfo{_UserName};
		if (UserState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo) > SaveDatabaseContinuation;
		else
		{
			UserState.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo) > SaveDatabaseContinuation;
		}
		return SaveDatabaseContinuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_RemoveUser(NStr::CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			return DMibErrorInstance("No user with ID '{}'"_f << _UserID);

		auto &User = *pUser;
		TCContinuation<void> SaveDatabaseContinuation;

		if (User.m_bExistsInDatabase)
		{
			User.m_bExistsInDatabase = false;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveUser, _UserID) > SaveDatabaseContinuation;
		}
		else
			SaveDatabaseContinuation.f_SetResult();
		Internal.m_Users.f_Remove(_UserID);

		return SaveDatabaseContinuation;
	}

	TCContinuation<void> CDistributedActorTrustManager::f_SetUserInfo
		(
			CStr const &_UserID
			, TCOptional<CStr> const &_UserName
			, TCSet<CStr> const &_RemoveMetadata
			, TCMap<CStr, CEJSON> const &_AddMetadata
		)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		TCContinuation<void> SaveDatabaseContinuation;
		auto &UserState = *pUser;

		// Make a copy so we don't accidentally overwrite the database
		auto UserInfo = UserState.m_UserInfo;
		if (_UserName)
		{
			if (_UserName->f_IsEmpty())
				return DMibErrorInstance("User name cannot be empty");

			UserInfo.m_UserName = *_UserName;
		}

		for (auto const &Key : _RemoveMetadata)
		{
			auto pValue = UserInfo.m_Metadata.f_FindEqual(Key);
			if (!pValue)
				return DMibErrorInstance("Key '{}' does not exist"_f << Key);

			UserInfo.m_Metadata.f_Remove(pValue);
		}

		for (auto const &Metadata : _AddMetadata)
		{
			auto const &Key = _AddMetadata.fs_GetKey(Metadata);
			if (!Metadata.f_IsValid())
				return DMibErrorInstance("Invalid metadata for key '{}'"_f << Key);

			UserInfo.m_Metadata[Key] = Metadata;
		}
		UserState.m_UserInfo = fg_Move(UserInfo);
		if (UserState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo) > SaveDatabaseContinuation;
		else
		{
			UserState.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo) > SaveDatabaseContinuation;
		}
		return SaveDatabaseContinuation;
	}
}
