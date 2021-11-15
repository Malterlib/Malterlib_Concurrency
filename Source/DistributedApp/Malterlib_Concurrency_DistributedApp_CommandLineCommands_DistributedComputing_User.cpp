// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>
#include <Mib/Encoding/JSONShortcuts>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NCommandLine;
	using namespace NStr;
	using namespace NEncoding;
	using namespace NStorage;
	using namespace NContainer;

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AddUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		auto UserName = _Params["UserName"].f_String();
		auto UserID = NCryptography::fg_RandomID();

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_AddUser, UserID, UserName);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Add user ID '{}' ({}) to host from command line", UserID, UserName);

		if (!_Params["Quiet"].f_Boolean())
			*_pCommandLine += "{}\n"_f << UserID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
	{
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveUser, _UserID);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove user '{}' from command line", _UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ListUsers(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_TableType)
	{
		TCMap<CStr, CDistributedActorTrustManagerInterface::CUserInfo> Users = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUsers, false);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("User ID", "User Name");

		for (auto &UserInfo : Users)
			TableRenderer.f_AddRow(Users.fs_GetKey(UserInfo), UserInfo.m_UserName);

		TableRenderer.f_Output(_TableType);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Reported users to command line");

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetUserInfo(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		CStr const &UserID = _Params["UserID"].f_String();
		TCOptional<CStr> UserName;
		TCMap<CStr, CEJSON> AddMetadata;
		TCSet<CStr> RemoveMetadata;

		if (auto pValue = _Params.f_GetMember("UserName"))
			UserName = pValue->f_String();

		if (auto pValue = _Params.f_GetMember("Metadata"))
		{
			auto Object = pValue->f_Object();
			for (auto iMetadata = Object.f_OrderedIterator(); iMetadata; ++iMetadata)
				AddMetadata[iMetadata->f_Name()] = iMetadata->f_Value();
		}

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo,  UserID, UserName, RemoveMetadata, AddMetadata);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Set metadata for user '{}' from command line", UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RemoveMetadata(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, CStr const &_Key)
	{
		TCOptional<CStr> UserName;
		TCMap<CStr, CEJSON> AddMetadata;
		TCSet<CStr> RemoveMetadata{_Key};

		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetUserInfo, _UserID, UserName, AddMetadata, RemoveMetadata);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Remove metadata '{}' from user '{}' from command line", _Key, _UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ExportUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, bool _bIncludePrivate)
	{
		CStr UserData = co_await fg_ExportUser(mp_State.m_TrustManager, _UserID, _bIncludePrivate);

		*_pCommandLine += "{}\n"_f << UserData;

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Exported user ID '{}' from command line", _UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_ImportUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserData)
	{
		CStr UserID = co_await fg_ImportUser(mp_State.m_TrustManager, _UserData);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Import user ID '{}' from command line", UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_GetDefaultUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine)
	{
		CStr UserID = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_GetDefaultUser);

		*_pCommandLine += "{}\n"_f << UserID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_SetDefaultUser(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID)
	{
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_SetDefaultUser, _UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_RegisterAuthenticationFactor
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_UserID
			, CStr const &_Factor
			, bool _bQuiet
		)
	{
		CStr FactorID = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RegisterUserAuthenticationFactor, _pCommandLine, _UserID, _Factor);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Added authentication factor '{}' of type '{}' to user '{}' from command line", FactorID, _Factor, _UserID);

		if (!_bQuiet)
			*_pCommandLine += "{}\n"_f << FactorID;

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_UnregisterAuthenticationFactor
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_UserID
			, CStr const &_Factor
		)
	{
		co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_RemoveUserAuthenticationFactor, _UserID, _Factor);

		DMibLogWithCategory(Mib/Concurrency/App, Info, "Removed authentication factor '{}' from user '{}' from command line", _Factor, _UserID);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_EnumUserAuthenticationFactors(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_UserID, CStr const &_TableType)
	{
		TCMap<CStr, CAuthenticationData> Factors = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumUserAuthenticationFactors, _UserID);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("Factor ID", "Factor Name");

		for (auto &Factor : Factors)
			TableRenderer.f_AddRow(Factors.fs_GetKey(Factor), Factor.m_Name);

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_EnumAuthenticationFactors(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CStr const &_TableType)
	{
		TCMap<CStr, CAuthenticationActorInfo> Factors = co_await mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors);

		CTableRenderHelper TableRenderer = _pCommandLine->f_TableRenderer();
		TableRenderer.f_AddHeadings("Factor Name", "Category");

		for (auto &Factor : Factors)
			TableRenderer.f_AddRow(Factors.fs_GetKey(Factor), fg_AuthenticationFactorCategoryToString(Factor.m_Category));

		TableRenderer.f_Output(_TableType);

		co_return 0;
	}

	TCFuture<uint32> CDistributedAppActor::f_CommandLine_AuthenticatePermissionPattern(TCSharedPointer<CCommandLineControl> const &_pCommandLine, CEJSON const &_Params)
	{
		if (!mp_AuthenticationHandlerImplementation)
		{
			if (!mp_Settings.m_bSupportUserAuthentication)
				co_return DMibErrorInstance("Authentication is not supported in this application");
			else
				co_return DMibErrorInstance("Authentication not supported. Set default authentication user or use --authentication-user.");
		}

		CStr Pattern = _Params["Pattern"].f_String();
		bool bJSONOutput = _Params["JSONOutput"].f_Boolean();

		TCSet<CStr> Factors;
		CEJSON const &AuthenticationFactors = _Params["AuthenticationFactors"];
		if (AuthenticationFactors.f_IsString())
			Factors[AuthenticationFactors.f_String()];
		else
		{
			for (auto &Factor : AuthenticationFactors.f_Array())
				Factors[Factor.f_String()];
		}

		struct CCollectedAuthenticationActorInfo
		{
			TCDistributedActor<ICDistributedActorAuthentication> m_Actor;
			CStr m_RemoteHostID;
			CStr m_Description;
		};

		TCVector<CCollectedAuthenticationActorInfo> AuthenticationActors;
		for (auto const &RegisteredAuthentication : mp_AuthenticationRegistrationSubscriptions)
		{
			auto const &WeakActor = mp_AuthenticationRegistrationSubscriptions.fs_GetKey(RegisteredAuthentication);

			auto Actor = WeakActor.f_Lock();
			if (!Actor)
				continue;
			AuthenticationActors.f_Insert({Actor, RegisteredAuthentication.m_ActorInfo.m_HostInfo.m_HostID, RegisteredAuthentication.m_ActorInfo.m_HostInfo.f_GetDesc()});
		}

		auto [MultipleRequestData, FactorActors] = co_await
			(
				mp_AuthenticationHandlerImplementation(&ICDistributedActorAuthenticationHandler::f_GetMultipleRequestSubscription, AuthenticationActors.f_GetLen())
				+ mp_State.m_TrustManager(&CDistributedActorTrustManager::f_EnumAuthenticationActors)
			)
		;

		for (auto const &Factor : Factors)
		{
			if (!FactorActors.f_FindEqual(Factor))
				co_return DMibErrorInstance("No authentication factor named '{}'"_f << Factor);
		}

		TCActorResultVector<bool> AuthenticationResults;
		for (auto const &AuthenticationActorInfo : AuthenticationActors)
		{
			AuthenticationActorInfo.m_Actor.f_CallActor(&ICDistributedActorAuthentication::f_AuthenticatePermissionPattern)
				(
					Pattern
					, Factors
					, MultipleRequestData.m_ID
				)
				.f_Timeout(10.0, "Timeout waiting for manager to reply")
				> AuthenticationResults.f_AddResult()
			;
		}

		TCVector<TCAsyncResult<bool>> Authentications = co_await AuthenticationResults.f_GetResults();

		aint ReturnValue = 0;
		DMibCheck(AuthenticationActors.f_GetLen() == Authentications.f_GetLen());

		CEJSON JSONOutput;

		if (bJSONOutput)
			JSONOutput["AuthenticationFailures"] = EJSONType_Array;

		auto iAuthenticationActorInfo = AuthenticationActors.f_GetIterator();
		for (auto &Result : Authentications)
		{
			auto &AuthenticationActorInfo = *iAuthenticationActorInfo;
			++iAuthenticationActorInfo;

			if (!Result)
			{
				if (bJSONOutput)
				{
					JSONOutput["AuthenticationFailures"].f_Array().f_Insert() =
						{
							"RemoteHostID"_= AuthenticationActorInfo.m_RemoteHostID
							, "RemoteHostDescription"_= AuthenticationActorInfo.m_Description
							, "Exception"_= Result.f_GetExceptionStr()
						}
					;
				}
				else
					*_pCommandLine %= "[{}] Failed authenticate pattern. Exception: {}\n"_f << AuthenticationActorInfo.m_Description << Result.f_GetExceptionStr();

				ReturnValue = 1;
				continue;
			}

			if (!*Result)
			{
				if (bJSONOutput)
				{
					JSONOutput["AuthenticationFailures"].f_Array().f_Insert() =
						{
							"RemoteHostID"_= AuthenticationActorInfo.m_RemoteHostID
							, "RemoteHostDescription"_= AuthenticationActorInfo.m_Description
						}
					;
				}
				else
					*_pCommandLine %= "[{}] Failed to authenticate.\n"_f << AuthenticationActorInfo.m_Description;
				ReturnValue = 1;
			}
		}
		if (bJSONOutput)
			*_pCommandLine += JSONOutput.f_ToString();

		co_return ReturnValue;
	}
}
