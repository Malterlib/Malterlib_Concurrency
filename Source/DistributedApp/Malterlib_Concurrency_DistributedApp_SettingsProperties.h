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

	enum EDefaultCommandLineFunctionality
	{
		EDefaultCommandLineFunctionality_None = 0
		, EDefaultCommandLineFunctionality_Help = DMibBit(0)
		, EDefaultCommandLineFunctionality_Logging = DMibBit(1)
		, EDefaultCommandLineFunctionality_DistributedComputing = DMibBit(2)
		, EDefaultCommandLineFunctionality_Authentication = DMibBit(3)
		, EDefaultCommandLineFunctionality_Terminal = DMibBit(4)
		, EDefaultCommandLineFunctionality_Sensor = DMibBit(5)

		, EDefaultCommandLineFunctionality_AllNoDistributedComputing
		= EDefaultCommandLineFunctionality_Help
		| EDefaultCommandLineFunctionality_Logging
		| EDefaultCommandLineFunctionality_Terminal

		, EDefaultCommandLineFunctionality_All
		= EDefaultCommandLineFunctionality_Help
		| EDefaultCommandLineFunctionality_Logging
		| EDefaultCommandLineFunctionality_DistributedComputing
		| EDefaultCommandLineFunctionality_Authentication
		| EDefaultCommandLineFunctionality_Terminal
		| EDefaultCommandLineFunctionality_Sensor
	};

	struct CDistributedAppActor_InterfaceSettings
	{
		enum EOption
		{
			EOption_None = 0
			, EOption_DelegateTrustToAppInterface = DMibBit(0)
		};

		static EOption fs_ParseEnvironmentOptions(NStr::CStr const &_Options);

		NStorage::TCSharedPointer<TCActorFunctor<TCFuture<CDistributedActorTrustManager::CTrustTicket> ()>> m_pRequestTicket;
		NStr::CStr m_ServerAddress = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerAddress");
		NStr::CStr m_RequestTicketMagic = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerRequestTicket");
		NStr::CStr m_LaunchID = fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerLaunchID");
		EOption m_Options = fs_ParseEnvironmentOptions(fg_GetSys()->f_GetProtectedEnvironmentVariable("MalterlibDistributedAppInterfaceServerOptions"));
	};

	struct CDistributedAppActor_SettingsProperties
	{
		CDistributedAppActor_SettingsProperties() = default;

		NStr::CStr m_AppName;
		NStr::CStr m_RootDirectory = NFile::CFile::fs_GetProgramDirectory();
		NStr::CStr m_FriendlyName;
		NStr::CStr m_AuditCategory;
		NStr::CStr m_RunAsUser;
		NStr::CStr m_RunAsGroup;
		NStr::CStr m_Enclave;
		NCryptography::CPublicKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
		NNetwork::ENetFlag m_ListenFlags = NNetwork::ENetFlag_None;
		EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
		CDistributedAppActor_InterfaceSettings m_InterfaceSettings;
		EDefaultCommandLineFunctionality m_DefaultCommandLineFunctionality = EDefaultCommandLineFunctionality_All;

		bool m_bSeparateDistributionManager = false;
		bool m_bSupportUserAuthentication = true;
		bool m_bWaitForRemotes = true;
		bool m_bCanUserLocalListenAsPrimary = true;
		bool m_bCommandLineBeforeAppStart = false;
	};
}
