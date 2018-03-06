// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"

namespace NMib::NConcurrency
{
	using namespace NContainer;
	using namespace NStr;
	using namespace NPtr;
	using namespace NCryptography;

	TCContinuation<TCMap<CStr, CAuthenticationData>> CDistributedActorTrustManager::f_EnumUserAuthenticationFactors(NStr::CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		TCMap<CStr, CAuthenticationData> Result;
		if (auto *pFactors = Internal.m_AuthenticationFactors.f_FindEqual(_UserID))
		{
			for (auto &Factor : *pFactors)
				Result[pFactors->fs_GetKey(Factor)] = Factor.m_AuthenticationFactor;
		}
		return fg_Explicit(Result);
	}

	TCContinuation<void> CDistributedActorTrustManager::f_AddAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CAuthenticationData &&_Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_AuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (pAuthenticationFactorState)
			return DMibErrorInstance("User '{}' already has factor '{}' registered"_f << _UserID << _FactorID);

		auto &AuthenticationFactorState = Internal.m_AuthenticationFactors[_UserID][_FactorID];
		AuthenticationFactorState.m_AuthenticationFactor = fg_Move(_Data);
		AuthenticationFactorState.m_bExistsInDatabase = true;

		return Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddAuthenticationFactor, _UserID, _FactorID, AuthenticationFactorState.m_AuthenticationFactor);
	}

	TCContinuation<void> CDistributedActorTrustManager::f_SetAuthenticationFactor(CStr const &_UserID, CStr const &_FactorID, CAuthenticationData &&_Data)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		auto *pAuthenticationFactorState = Internal.m_AuthenticationFactors[_UserID].f_FindEqual(_FactorID);
		if (!pAuthenticationFactorState)
			return DMibErrorInstance("User '{}' does not have factor '{}' registered"_f << _UserID << _FactorID);

		pAuthenticationFactorState->m_AuthenticationFactor = fg_Move(_Data);
		pAuthenticationFactorState->m_bExistsInDatabase = true;

		return Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetAuthenticationFactor, _UserID, _FactorID, pAuthenticationFactorState->m_AuthenticationFactor);
	}

	TCContinuation<void> CDistributedActorTrustManager::f_RemoveAuthenticationFactor(NStr::CStr const &_UserID, NStr::CStr const &_FactorID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;

		if (!Internal.m_Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' does not exist"_f << _UserID);

		if (auto *pUser = Internal.m_AuthenticationFactors.f_FindEqual(_UserID))
		{
			if (!pUser->f_FindEqual(_FactorID))
				return DMibErrorInstance("User '{}' does not have an authentication factor of type '{}' registered"_f << _UserID << _FactorID);
		}

		TCContinuation<void> Continuation;
		auto &AuthenticationFactorState = Internal.m_AuthenticationFactors[_UserID][_FactorID];
		if (AuthenticationFactorState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveAuthenticationFactor, _UserID, _FactorID) > Continuation;
		else
			Continuation.f_SetResult();

		Internal.m_AuthenticationFactors[_UserID].f_Remove(_FactorID);
		if (Internal.m_AuthenticationFactors[_UserID].f_IsEmpty())
			Internal.m_AuthenticationFactors.f_Remove(_UserID);
		return Continuation;
	}

	TCContinuation<CStr> CDistributedActorTrustManager::f_RegisterAuthenticationFactor
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

		auto *pActor = Internal.m_AuthenticationActors.f_FindEqual(_Factor);
		if (!pActor)
			return DMibErrorInstance("No authentication factor matching '{}'"_f << _Factor);

		TCContinuation<CStr> Continuation;
		auto &Actor = *pActor;
		Actor(&ICDistributedActorTrustManagerAuthenticationActor::f_RegisterFactor, _UserID, _pCommandLine) > Continuation / [this, Continuation, _UserID]
			(
				CAuthenticationData &&_Result
			)
			{
				CStr ID = fg_RandomID();
				fg_ThisActor(this)(&CDistributedActorTrustManager::f_AddAuthenticationFactor, _UserID, ID, fg_Move(_Result)) > Continuation / [Continuation, ID]
					{
						Continuation.f_SetResult(ID);
					}
				;
			}
		;
		return Continuation;
	}
}




