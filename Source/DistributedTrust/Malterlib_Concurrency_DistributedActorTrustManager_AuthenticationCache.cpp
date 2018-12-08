// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/Actor/Timer>

#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationCache.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NStorage;
	using namespace NContainer;

	void CDistributedActorTrustManagerAuthenticationCache::f_AddAuthenticatedPermission
		(
			CPermissionIdentifiers const &_Identity
			, CStr const &_Permission
			, NTime::CTime const &_ExpirationTime
		 	, NTime::CTime const &_CacheTime
		)
	{
		DMibRequire(_Identity.f_GetHostID() && _Identity.f_GetUserID());
		DMibRequire(_CacheTime.f_IsValid());
		DMibRequire(_ExpirationTime.f_IsValid());
		DMibRequire(_ExpirationTime != NTime::CTime::fs_EndOfTime());

		auto &Permissions = mp_AuthenticatedPermissions[_Identity];
		auto &CacheLifetimes = Permissions[_Permission];
		CacheLifetimes.m_ExpirationTime = _ExpirationTime;
		CacheLifetimes.m_CacheTime = _CacheTime;
		f_StartPeriodicCleanup();
	}

	void CDistributedActorTrustManagerAuthenticationCache::f_RemoveAuthenticatedPermission(CPermissionIdentifiers const &_Identity, NStr::CStr const &_Permission)
	{
		if (!_Identity.f_GetUserID())
			return;

		if (_Identity.f_GetHostID())
		{
			if (auto pPermissions = mp_AuthenticatedPermissions.f_FindEqual(_Identity))
			{
				pPermissions->f_Remove(_Permission);
				if (pPermissions->f_IsEmpty())
					mp_AuthenticatedPermissions.f_Remove(pPermissions);
			}
		}
		else
		{
			auto &UserID = _Identity.f_GetUserID();

			for (auto iPermissions = mp_AuthenticatedPermissions.f_GetIterator(); iPermissions;)
			{
				if (iPermissions.f_GetKey().f_GetUserID() != UserID)
					continue;

				iPermissions->f_Remove(_Permission);
				if (iPermissions->f_IsEmpty())
					iPermissions.f_Remove();
				else
					++iPermissions;
			}
		}
	}

	namespace
	{
		NTime::CTime fg_GetPermissionExpireTime(NTime::CTime const &_CacheTime, int64 _MaximumAuthenticationLifetime)
		{
			if (_MaximumAuthenticationLifetime == CPermissionRequirements::mc_OverrideLifetimeNotSet)
				return NTime::CTime::fs_EndOfTime();
			else
				return _CacheTime + NTime::CTimeSpanConvert::fs_CreateMinuteSpan(_MaximumAuthenticationLifetime);
		}
	}

	bool CDistributedActorTrustManagerAuthenticationCache::f_HasAuthenticatedPermission
		(
			CPermissionIdentifiers const &_Identity
			, CStr const &_Permission
			, bool _bIsWildcard
		 	, int64 _MaximumAuthenticationLifetime
		)
	{
		DMibRequire(_Identity.f_GetHostID() && _Identity.f_GetUserID());

		auto *pPermissions = mp_AuthenticatedPermissions.f_FindEqual(_Identity);
		if (!pPermissions)
			return false;

		auto Now = NTime::CTime::fs_NowUTC();

		if (_bIsWildcard)
		{
			for (auto &CacheLifetimes : *pPermissions)
			{
				auto &Permission = pPermissions->fs_GetKey(CacheLifetimes);
				if (NStr::fg_StrMatchWildcard(_Permission.f_GetStr(), Permission.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
				{
					if (fg_Min(CacheLifetimes.m_ExpirationTime, fg_GetPermissionExpireTime(CacheLifetimes.m_CacheTime, _MaximumAuthenticationLifetime)) >= Now)
						return true;
				}
			}
		}
		else if (auto pCacheLifetimes = pPermissions->f_FindEqual(_Permission))
			return fg_Min(pCacheLifetimes->m_ExpirationTime, fg_GetPermissionExpireTime(pCacheLifetimes->m_CacheTime, _MaximumAuthenticationLifetime)) >= Now;

		return false;
	}

	void CDistributedActorTrustManagerAuthenticationCache::f_SetAuthenticatedPermissionPattern
		(
			CPermissionIdentifiers const &_Identity
			, CStr &&_Pattern
			, TCSet<CStr> &&_AuthenticationFactors
			, NTime::CTime const &_ExpirationTime
		 	, NTime::CTime const &_CacheTime
		)
	{
		DMibRequire(_ExpirationTime.f_IsValid());
		DMibRequire(_CacheTime.f_IsValid());
		DMibRequire(_Identity.f_GetHostID() && _Identity.f_GetUserID());

		auto &Pattern = mp_PreauthenticatedPatterns[_Identity][_Pattern];
		for (auto const &Factor : _AuthenticationFactors)
		{
			auto &CacheLifetimes = Pattern[Factor];
			CacheLifetimes.m_ExpirationTime = _ExpirationTime;
			CacheLifetimes.m_CacheTime = _CacheTime;
		}
		f_StartPeriodicCleanup();
	}

	TCSet<CStr> CDistributedActorTrustManagerAuthenticationCache::f_HasAuthenticatedPermissionPattern
		(
		 	CPermissionIdentifiers const &_Identity
		 	, CStr const &_Permission
		 	, int64 _MaximumAuthenticationLifetime
		)
	{
		DMibRequire(_Identity.f_GetHostID() && _Identity.f_GetUserID());

		auto *pPatterns = mp_PreauthenticatedPatterns.f_FindEqual(_Identity);
		if (!pPatterns)
			return {};

		auto Now = NTime::CTime::fs_NowUTC();

		TCSet<CStr> Result;
		for (auto &ExpirationtimePerFactor : *pPatterns)
		{
			auto const &Pattern = pPatterns->fs_GetKey(ExpirationtimePerFactor);
			if (NStr::fg_StrMatchWildcard(_Permission.f_GetStr(), Pattern.f_GetStr()) == NStr::EMatchWildcardResult_WholeStringMatchedAndPatternExhausted)
			{
				for (auto const &CacheLifetimes : ExpirationtimePerFactor)
				{
					DMibCheck(CacheLifetimes.m_ExpirationTime.f_IsValid());
					DMibCheck(CacheLifetimes.m_CacheTime.f_IsValid());

					if (fg_Min(CacheLifetimes.m_ExpirationTime, fg_GetPermissionExpireTime(CacheLifetimes.m_CacheTime, _MaximumAuthenticationLifetime)) >= Now)
						Result[ExpirationtimePerFactor.fs_GetKey(CacheLifetimes)];
				}
			}
		}
		return Result;
	}

	TCContinuation<void> CDistributedActorTrustManagerAuthenticationCache::f_CleanupExpired()
	{
		auto Now = NTime::CTime::fs_NowUTC();

		auto iPermissions = mp_AuthenticatedPermissions.f_GetIterator();
		while (iPermissions)
		{
			auto iCacheLifetimes = iPermissions->f_GetIterator();
			while (iCacheLifetimes)
			{
				DMibCheck(iCacheLifetimes->m_ExpirationTime.f_IsValid());
				if (iCacheLifetimes->m_ExpirationTime < Now)
					iCacheLifetimes.f_Remove();
				else
					++iCacheLifetimes;
			}

			if (iPermissions->f_IsEmpty())
				iPermissions.f_Remove();
			else
				++iPermissions;
		}

		auto iPatterns = mp_PreauthenticatedPatterns.f_GetIterator();
		while (iPatterns)
		{
			auto iFactors = iPatterns->f_GetIterator();
			while (iFactors)
			{
				auto iCacheLifetimes = iFactors->f_GetIterator();
				while (iCacheLifetimes)
				{
					DMibCheck(iCacheLifetimes->m_ExpirationTime.f_IsValid());
					if (iCacheLifetimes->m_ExpirationTime < Now)
						iCacheLifetimes.f_Remove();
					else
						++iCacheLifetimes;
				}

				if (iFactors->f_IsEmpty())
					iFactors.f_Remove();
				else
					++iFactors;
			}

			if (iPatterns->f_IsEmpty())
				iPatterns.f_Remove();
			else
				++iPatterns;
		}

		if (mp_AuthenticatedPermissions.f_IsEmpty() && mp_PreauthenticatedPatterns.f_IsEmpty())
			mp_PeriodicCleanupSubscription.f_Clear();

		return fg_Explicit();
	}

	void CDistributedActorTrustManagerAuthenticationCache::f_StartPeriodicCleanup()
	{
		if (mp_PeriodicCleanupSubscription.f_IsEmpty())
		{
			fg_RegisterTimer
				(
					15.0 * 60.0
					, fg_CurrentActor()
					, [this]
					{
						return f_CleanupExpired();
					}
				)
				> [this](TCAsyncResult<CActorSubscription> &&_Subscription)
				{
					if (!_Subscription)
						DMibLogWithCategory(Malterlib/Concurrency/AppManager, Info, "Failed to start periodic cache cleanup {}", _Subscription.f_GetExceptionStr());

					mp_PeriodicCleanupSubscription = fg_Move(*_Subscription);
				}
			;
		}
	}
}
