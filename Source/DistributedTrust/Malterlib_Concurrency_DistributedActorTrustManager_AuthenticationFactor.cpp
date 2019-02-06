// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/DistributedApp>
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NContainer;
	using namespace NStr;
	using namespace NStorage;
	using namespace NCryptography;

	TCFuture<TCMap<CStr, CAuthenticationData>> CDistributedActorTrustManager::f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID) const
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		TCMap<CStr, CAuthenticationData> Result;
		if (auto *pFactors = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			for (auto &Factor : *pFactors)
				Result[pFactors->fs_GetKey(Factor)] = Factor.m_AuthenticationFactor;
		}
		return fg_Explicit(Result);
	}

	TCFuture<void> CDistributedActorTrustManager::f_AddUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CAuthenticationData &&_Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (pAuthenticationFactorState)
			return DMibErrorInstance("User '{}' already has factor '{}' registered"_f << _UserID << _FactorID);

		auto &AuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID][_FactorID];
		AuthenticationFactorState.m_AuthenticationFactor = fg_Move(_Data);
		AuthenticationFactorState.m_bExistsInDatabase = true;

		return Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUserAuthenticationFactor, _UserID, _FactorID, AuthenticationFactorState.m_AuthenticationFactor);
	}

	TCFuture<void> CDistributedActorTrustManager::f_SetUserAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CAuthenticationData &&_Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (!pAuthenticationFactorState)
			return DMibErrorInstance("User '{}' does not have factor '{}' registered"_f << _UserID << _FactorID);

		pAuthenticationFactorState->m_AuthenticationFactor = fg_Move(_Data);
		pAuthenticationFactorState->m_bExistsInDatabase = true;

		return Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserAuthenticationFactor, _UserID, _FactorID, pAuthenticationFactorState->m_AuthenticationFactor);
	}

	TCFuture<void> CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		if (auto *pUser = Internal.m_UserAuthenticationFactors.f_FindEqual(_UserID))
		{
			if (!pUser->f_FindEqual(_FactorID))
				return DMibErrorInstance("User '{}' does not have an authentication factor of type '{}' registered"_f << _UserID << _FactorID);
		}

		TCPromise<void> Promise;
		auto &AuthenticationFactorState = Internal.m_UserAuthenticationFactors[_UserID][_FactorID];
		if (AuthenticationFactorState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveUserAuthenticationFactor, _UserID, _FactorID) > Promise;
		else
			Promise.f_SetResult();

		Internal.m_UserAuthenticationFactors[_UserID].f_Remove(_FactorID);
		if (Internal.m_UserAuthenticationFactors[_UserID].f_IsEmpty())
			Internal.m_UserAuthenticationFactors.f_Remove(_UserID);
		return Promise.f_MoveFuture();
	}

	TCFuture<CStr> CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_UserID
			, CStr const &_Factor
		)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pActorInfo = Internal.m_AuthenticationActors.f_FindEqual(_Factor);
		if (!pActorInfo)
			return DMibErrorInstance("No authentication factor matching '{}'"_f << _Factor);

		TCPromise<CStr> Promise;
		auto &Actor = pActorInfo->m_Actor;
		Actor(&ICDistributedActorTrustManagerAuthenticationActor::f_RegisterFactor, _UserID, _pCommandLine) > Promise / [this, Promise, _UserID]
			(
				CAuthenticationData &&_Result
			)
			{
				CStr ID = fg_RandomID();
				fg_ThisActor(this)(&CDistributedActorTrustManager::f_AddUserAuthenticationFactor, _UserID, ID, fg_Move(_Result)) > Promise / [Promise, ID]
					{
						Promise.f_SetResult(ID);
					}
				;
			}
		;
		return Promise.f_MoveFuture();
	}
}




