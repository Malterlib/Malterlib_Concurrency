// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ActorFunctor>

namespace NMib::NConcurrency
{
	enum EDistributedAppType
	{
		EDistributedAppType_Unknown
		, EDistributedAppType_Unchanged
		, EDistributedAppType_InProcess
		, EDistributedAppType_Daemon
		, EDistributedAppType_Local
		, EDistributedAppType_ForceLocal
		, EDistributedAppType_CommandLine
		, EDistributedAppType_DirectCommandLine
	};
	
	enum EDistributedAppUpdateType
	{
		EDistributedAppUpdateType_Independent
		, EDistributedAppUpdateType_OneAtATime
		, EDistributedAppUpdateType_AllAtOnce
	};

	struct CDistributedAppActor_InterfaceSettings
	{
		enum EOption
		{
			EOption_None = 0
			, EOption_DelegateTrustToAppInterface = DMibBit(0)
		};

		static EOption fs_ParseEnvironmentOptions(NStr::CStr const &_Options);

		NPtr::TCSharedPointer<TCActorFunctor<TCContinuation<CDistributedActorTrustManager::CTrustTicket> ()>> m_pRequestTicket;
		NStr::CStr m_ServerAddress = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerAddress");
		NStr::CStr m_RequestTicketMagic = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerRequestTicket");
		EOption m_Options = fs_ParseEnvironmentOptions(fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerOptions"));
	};

	struct CDistributedAppActor_SettingsProperties
	{
		CDistributedAppActor_SettingsProperties() = default;
		CDistributedAppActor_SettingsProperties
			(
				NStr::CStr const &_AppName
				, bool _bRequireListen
				, NStr::CStr const &_RootDirectory
				, bool _bSeparateDistributionManager
				, NNet::CSSLKeySetting _KeySetting
				, NStr::CStr const &_FriendlyName
				, NStr::CStr const &_Enclave
				, EDistributedAppUpdateType _UpdateType
				, NStr::CStr const &_AuditCategory
				, CDistributedAppActor_InterfaceSettings const &_InterfaceSettings
			 	, bool _bSupportUserAuthentication
			)
		;

		NStr::CStr m_AppName;
		NStr::CStr m_RootDirectory = NFile::CFile::fs_GetProgramDirectory();
		NStr::CStr m_FriendlyName;
		NStr::CStr m_AuditCategory;
		NStr::CStr m_Enclave;
		NNet::CSSLKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
		NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
		EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
		CDistributedAppActor_InterfaceSettings m_InterfaceSettings;
		bool m_bRequireListen = false;
		bool m_bSeparateDistributionManager = false;
		bool m_bSupportUserAuthentication = true;
	};
}
