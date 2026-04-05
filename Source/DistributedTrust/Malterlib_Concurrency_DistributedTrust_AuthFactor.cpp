// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/DistributedApp>
#include "Malterlib_Concurrency_DistributedTrust.h"
#include "Malterlib_Concurrency_DistributedTrust_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NContainer;
	using namespace NStr;
	using namespace NStorage;
	using namespace NCryptography;

	TCFuture<TCMap<CStr, CAuthenticationData>> CDistributedActorTrustManager::f_EnumUserAuthenticationFactors(NStr::CStr _UserID) const
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!Internal.m_Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		TCMap<CStr, CAuthenticationData> Result;
		if (auto *pFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			for (auto &Factor : *pFactors)
				Result[pFactors->fs_GetKey(Factor)] = Factor.m_AuthenticationFactor;
		}
		co_return fg_Move(Result);
	}

	TCFuture<void> CDistributedActorTrustManager::f_AddUserAuthenticationFactor(CStr _UserID, CStr _FactorID, CAuthenticationData _Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!Internal.m_Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (pAuthenticationFactorState)
			co_return DMibErrorInstance("User '{}' already has factor '{}' registered"_f << _UserID << _FactorID);

		auto &AuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID][_FactorID];
		AuthenticationFactorState.m_AuthenticationFactor = fg_Move(_Data);
		AuthenticationFactorState.m_bExistsInDatabase = true;

		co_return co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUserAuthenticationFactor, _UserID, _FactorID, AuthenticationFactorState.m_AuthenticationFactor);
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetUserAuthenticationFactor(CStr _UserID, CStr _FactorID, CAuthenticationData _Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!Internal.m_Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (!pAuthenticationFactorState)
			co_return DMibErrorInstance("User '{}' does not have factor '{}' registered"_f << _UserID << _FactorID);

		pAuthenticationFactorState->m_AuthenticationFactor = fg_Move(_Data);
		pAuthenticationFactorState->m_bExistsInDatabase = true;

		co_return co_await Internal.m_Database
			(
				&ICDistributedActorTrustManagerDatabase::f_SetUserAuthenticationFactor
				, _UserID
				, _FactorID
				, pAuthenticationFactorState->m_AuthenticationFactor
			)
		;
	}

	TCFuture<void> CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor(NStr::CStr _UserID, NStr::CStr _FactorID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!Internal.m_Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		if (auto *pUser = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (!pUser->f_FindEqual(_FactorID))
				co_return DMibErrorInstance("User '{}' does not have an authentication factor of type '{}' registered"_f << _UserID << _FactorID);
		}

		auto &AuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID][_FactorID];
		auto bExistsInDatabase = AuthenticationFactorState.m_bExistsInDatabase;

		Internal.m_UserAuthenticationFactors[_UserID].f_Remove(_FactorID);
		if (Internal.m_UserAuthenticationFactors[_UserID].f_IsEmpty())
			Internal.m_UserAuthenticationFactors.f_Remove(_UserID);

		if (bExistsInDatabase)
			co_await Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveUserAuthenticationFactor, _UserID, _FactorID);

		co_return {};
	}

	TCFuture<CStr> CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor
		(
			TCSharedPointer<CCommandLineControl> _pCommandLine
			, CStr _UserID
			, CStr _Factor
		)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			co_return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		co_await Internal.f_WaitForInit();

		if (!Internal.m_Users.f_FindEqual(_UserID))
			co_return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pActorInfo = Internal.m_AuthenticationActors.f_FindEqual(_Factor);
		if (!pActorInfo)
			co_return DMibErrorInstance("No authentication factor matching '{}'"_f << _Factor);

		CAuthenticationData Result = co_await pActorInfo->m_Actor(&ICDistributedActorTrustManagerAuthenticationActor::f_RegisterFactor, _UserID, _pCommandLine);
		CStr ID = fg_RandomID();
		co_await f_AddUserAuthenticationFactor(_UserID, ID, fg_Move(Result));

		co_return fg_Move(ID);
	}
}
