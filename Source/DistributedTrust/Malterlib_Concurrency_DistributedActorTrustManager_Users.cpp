// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Proxy.h"

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

	TCContinuation<TCOptional<CDistributedActorTrustManagerInterface::CUserInfo>> CDistributedActorTrustManager::f_TryGetUser(CStr const &_UserID)
	{
		auto &Internal = *mp_pInternal;

		if (auto *pUser = Internal.m_Users.f_FindEqual(_UserID))
			return fg_Explicit(CDistributedActorTrustManagerInterface::CUserInfo{pUser->m_UserInfo.m_UserName, pUser->m_UserInfo.m_Metadata});
		else
			return fg_Explicit();
	}

	TCContinuation<void> CDistributedActorTrustManager::f_AddUser(CStr const &_UserID, CStr const &_UserName)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		auto &Users = Internal.m_Users;

		if (Users.f_FindEqual(_UserID))
			return DMibErrorInstance("User '{}' already exists"_f << _UserID);

		TCContinuation<void> Continuation;
		auto &UserState = Internal.m_Users[_UserID];
		UserState.m_UserInfo = CUserInfo{_UserName};
		if (UserState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo) > Continuation;
		else
		{
			UserState.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo) > Continuation;
		}
		return Continuation;
	}
	
	TCContinuation<void> CDistributedActorTrustManager::f_RemoveUser(NStr::CStr const &_UserID)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		auto &Internal = *mp_pInternal;
		auto *pUser = Internal.m_Users.f_FindEqual(_UserID);
		if (!pUser)
			return DMibErrorInstance("No user with ID '{}'"_f << _UserID);

		bool bExistsInDatabase = pUser->m_bExistsInDatabase;

		TCActorResultVector<void> Results;
		if (auto *pFactors = Internal.m_AuthenticationFactors.f_FindEqual(_UserID))
		{
			TCSet<CStr> Factors;
			for (auto &Factor : *pFactors)
			{
				if (Factor.m_bExistsInDatabase)
					Factors[pFactors->fs_GetKey(Factor)];
			}
			for (auto &Factor : Factors)
				f_RemoveAuthenticationFactor(_UserID, Factor) > Results.f_AddResult();

			Internal.m_AuthenticationFactors.f_Remove(_UserID);
		}

		if (bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveUser, _UserID) > Results.f_AddResult();
		Internal.m_Users.f_Remove(_UserID);

		TCContinuation<void> Continuation;
		Results.f_GetResults() > Continuation / [Continuation](TCVector<TCAsyncResult<void>> &&_Results)
			{
				if (!fg_CombineResults(Continuation, fg_Move(_Results)))
					return;
				Continuation.f_SetResult();
			}
		;
		return Continuation;
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

		TCContinuation<void> Continuation;
		if (UserState.m_bExistsInDatabase)
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_SetUserInfo, _UserID, UserState.m_UserInfo) > Continuation;
		else
		{
			UserState.m_bExistsInDatabase = true;
			Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddUser, _UserID, UserState.m_UserInfo) > Continuation;
		}
		return Continuation;
	}
	template<typename t_CType>
	TCContinuation<CStr> fg_ExportUser(TCActor<t_CType> const &_TrustManager, CStr const &_UserID, bool _bIncludePrivate)
	{
		if (!CActorDistributionManager::fs_IsValidUserID(_UserID))
			return DMibErrorInstance("Invalid user ID");

		TCContinuation<CStr> Continuation;
		DMibCallActor(_TrustManager, t_CType::f_TryGetUser, _UserID) > Continuation
			/ [Continuation, _TrustManager, _UserID, _bIncludePrivate](TCOptional<CDistributedActorTrustManagerInterface::CUserInfo> const &_UserInfo) mutable
			{
				if (!_UserInfo)
					return Continuation.f_SetException(DMibErrorInstance("User {} does not exist"_f << _UserID));

				DMibCallActor(_TrustManager, t_CType::f_EnumUserAuthenticationFactors, _UserID)
					> Continuation / [Continuation, _UserID, UserInfo = *_UserInfo, _bIncludePrivate](NContainer::TCMap<NStr::CStr, CAuthenticationData> &&_Factors) mutable
					{
						TCContinuation<NStr::CStr>::fs_RunProtected<NException::CException>() >
							[&]() -> NStr::CStr
							{
								NStream::CBinaryStreamMemory<> Stream;
								Stream << _UserID;
								Stream << UserInfo;

								if (!_bIncludePrivate)
								{
									for (auto &Factor : _Factors)
										Factor.m_PrivateData.f_Clear();
								}

								Stream << _Factors;

								return NDataProcessing::fg_Base64Encode(Stream.f_GetVector());
							}
							> Continuation
						;
					}
				;
			}
		;

		return Continuation;
	}

	template<typename t_CType>
	TCContinuation<NStr::CStr> fg_ImportUser(TCActor<t_CType> const &_TrustManager, NStr::CStr const &_UserData)
	{
		struct CResult
		{
			NStr::CStr m_UserID;
			CDistributedActorTrustManagerInterface::CUserInfo m_UserInfo;
			NContainer::TCMap<NStr::CStr, CAuthenticationData> m_AuthenticationData;
		};

		TCContinuation<NStr::CStr> Continuation;

		TCContinuation<CResult>::template fs_RunProtected<NException::CException>() >
			[&]() -> CResult
			{
				CResult Result;
				NContainer::TCVector<uint8> Data;

				NDataProcessing::fg_Base64Decode(_UserData, Data);
				NStream::CBinaryStreamMemoryPtr<> Stream;
				Stream.f_OpenRead(Data);
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
				return Result;
			}
			> Continuation / [Continuation, _TrustManager] (CResult &&_Result)
			{
				NStr::CStr UserID = _Result.m_UserID;
				DMibCallActor(_TrustManager, t_CType::f_TryGetUser, UserID)
					> Continuation / [Continuation, _TrustManager, Result = fg_Move(_Result)](TCOptional<CDistributedActorTrustManagerInterface::CUserInfo> const &_UserInfo)
					{
						TCContinuation<void> AddContinuation;
						if (!_UserInfo)
							DMibCallActor(_TrustManager, t_CType::f_AddUser, Result.m_UserID, Result.m_UserInfo.m_UserName) > AddContinuation;
						else
							AddContinuation.f_SetResult();

						AddContinuation > Continuation / [Continuation, _TrustManager, Result]
							{
								DMibCallActor(_TrustManager, t_CType::f_SetUserInfo, Result.m_UserID, Result.m_UserInfo.m_UserName, NContainer::TCSet<CStr>{}, Result.m_UserInfo.m_Metadata)
									> Continuation / [Continuation, _TrustManager, Result]
									{
										DMibCallActor(_TrustManager, t_CType::f_EnumUserAuthenticationFactors, Result.m_UserID)
											> Continuation / [Continuation, _TrustManager, Result](NContainer::TCMap<CStr, CAuthenticationData> &&_Factors)
											{
												TCActorResultVector<void> Results;

												for (auto const &Data : Result.m_AuthenticationData)
												{
													auto FactorID = Result.m_AuthenticationData.fs_GetKey(Data);
													auto *pFactor = _Factors.f_FindEqual(FactorID);

													if (pFactor)
													{
														if (pFactor->m_PrivateData.f_IsEmpty() || !Data.m_PrivateData.f_IsEmpty())
														{
															DMibCallActor(_TrustManager, t_CType::f_SetAuthenticationFactor, Result.m_UserID, FactorID, fg_TempCopy(Data))
																> Results.f_AddResult()
															;
														}
													}
													else
													{
														DMibCallActor(_TrustManager, t_CType::f_AddAuthenticationFactor, Result.m_UserID, FactorID, fg_TempCopy(Data))
															> Results.f_AddResult();
														;
													}
												}

												Results.f_GetResults() > Continuation / [Continuation, UserID = Result.m_UserID](NContainer::TCVector<TCAsyncResult<void>> &&_Results)
													{
														if (!fg_CombineResults(Continuation, fg_Move(_Results)))
															return;
														Continuation.f_SetResult(UserID);
													}
												;
											}
										;
									}
								;
							}
						;
					}
				;
			}
		;
		return Continuation;
	}

	template TCContinuation<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserData);
	template TCContinuation<NStr::CStr> fg_ImportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	template TCContinuation<NStr::CStr> fg_ImportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserData);
	template TCContinuation<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManager> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	template TCContinuation<NStr::CStr> fg_ExportUser(TCActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
	template TCContinuation<NStr::CStr> fg_ExportUser(TCDistributedActor<CDistributedActorTrustManagerInterface> const &_TrustManager, NStr::CStr const &_UserID, bool _bIncludePrivate);
}
