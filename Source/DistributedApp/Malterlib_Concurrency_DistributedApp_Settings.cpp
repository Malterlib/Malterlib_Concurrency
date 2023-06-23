// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedApp.h"

#include <Mib/Cryptography/UUID>
#include <Mib/Cryptography/Hashes/SHA>
#include <Mib/Process/Platform>

namespace NMib::NConcurrency
{
	using namespace NEncoding;
	using namespace NFile;
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;
	using namespace NNetwork;
	using namespace NCryptography;

	namespace
	{
		struct CDefaultSettingsDefault
		{
			CDistributedAppActor_SettingsProperties m_Properties{CDistributedAppActor_SettingsProperties::CDefaultInit{}};
		};
	}

	NStorage::TCAggregate<CDefaultSettingsDefault> g_DefaultSettingsDefault = {DAggregateInit};

	CDistributedAppActor_SettingsProperties &CDistributedAppActor_Settings::fs_GetGlobalDefaultSettings()
	{
		return g_DefaultSettingsDefault->m_Properties;
	}

	CDistributedAppActor_SettingsProperties::CDistributedAppActor_SettingsProperties(CDefaultInit _Dummy)
	{
	}

	CDistributedAppActor_SettingsProperties::CDistributedAppActor_SettingsProperties()
	{
		*this = g_DefaultSettingsDefault->m_Properties;
	}

	namespace
	{
		CUniversallyUniqueIdentifier g_HostnameRootUUID("8ED61926-AC8A-4793-92C5-DE05547999E7", EUniversallyUniqueIdentifierFormat_Bare);
	}

	auto CDistributedAppActor_InterfaceSettings::fs_ParseEnvironmentOptions(CStr const &_Options) -> EOption
	{
		EOption OptionFlags = EOption_None;

		CStr Options = _Options;
		while (!Options.f_IsEmpty())
		{
			CStr Option = fg_GetStrSep(Options, ";");
			if (Option == "DelegateTrust")
				OptionFlags |= EOption_DelegateTrustToAppInterface;
		}

		return OptionFlags;
	}

	CDistributedAppActor_Settings::CDistributedAppActor_Settings()
		: CDistributedAppActor_SettingsProperties(fg_DistributedAppThreadLocal().m_DefaultSettings)
	{
	}

	CDistributedAppActor_Settings::CDistributedAppActor_Settings(NStr::CStr const &_AppName)
		: CDistributedAppActor_SettingsProperties(fg_DistributedAppThreadLocal().m_DefaultSettings)
	{
		m_AppName = _AppName;

		if (m_AuditCategory.f_IsEmpty())
			m_AuditCategory = _AppName;
#ifndef DPlatformFamily_Windows
		// A longer app name and enclave results in a unix socket name becoming too long 
		DMibRequire
			(
				fp_GetLocalSocketPath(fg_Format("/tmp/{}", g_HostnameRootUUID.f_GetAsString(EUniversallyUniqueIdentifierFormat_AlphaNum)), true, m_Enclave).f_GetLen()
				<= aint(NSys::NNetwork::fg_GetMaxUnixSocketNameLength())
			)
		;
#else
		if (!CFile::fs_GetDrive(m_RootDirectory).f_IsEmpty())
			m_RootDirectory = m_RootDirectory.f_Replace(CFile::fs_GetDrive(m_RootDirectory), CFile::fs_GetDrive(m_RootDirectory).f_LowerCase());
#endif
	}

	NStr::CStr CDistributedAppActor_Settings::fp_GetLocalSocketPath(CStr const &_Prefix, bool _bEnclaveSpecific, CStr const &_Enclave) const
	{
		if (!m_Enclave.f_IsEmpty() && _bEnclaveSpecific)
			return fg_Format("{}/{}.{}.socket", _Prefix, m_AppName, _Enclave);
		else
			return fg_Format("{}/{}.socket", _Prefix, m_AppName);
	}

	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketFileName(bool _bEnclaveSpecific, NStr::CStr const &_Enclave) const
	{
		mint MaxLength = NSys::NNetwork::fg_GetMaxUnixSocketNameLength();
		if (fp_GetLocalSocketPath(m_RootDirectory, true, m_Enclave).f_GetLen() <= aint(MaxLength))
			return fp_GetLocalSocketPath(m_RootDirectory, _bEnclaveSpecific, _Enclave);
		
		CStr ConfigHash = fg_GetHashedUuidString(m_RootDirectory, g_HostnameRootUUID, EUniversallyUniqueIdentifierFormat_AlphaNum);
		CStr TempDir = CFile::fs_GetRawTemporaryDirectory();
		CStr Prefix = TempDir / ConfigHash;
		if (fp_GetLocalSocketPath(Prefix, true, m_Enclave).f_GetLen() <= aint(MaxLength))
			return fp_GetLocalSocketPath(Prefix, _bEnclaveSpecific, _Enclave);
		
		Prefix = fg_Format("/tmp/{}", ConfigHash);
		DMibCheck(fp_GetLocalSocketPath(Prefix, true, m_Enclave).f_GetLen() <= aint(MaxLength));
		return fp_GetLocalSocketPath(Prefix, _bEnclaveSpecific, _Enclave);
	}
	
	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketWildcard(bool _bEnclaveSpecific) const
	{
		return f_GetLocalSocketFileName(_bEnclaveSpecific, "*");
	}
	
	NStr::CStr CDistributedAppActor_Settings::f_GetLocalSocketHostname(bool _bEnclaveSpecific) const
	{
		return fg_Format("UNIX(666):{}", f_GetLocalSocketFileName(_bEnclaveSpecific, m_Enclave));
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RootDirectory(CStr const &_RootDirectory) &&
	{
		m_RootDirectory = _RootDirectory;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_SeparateDistributionManager(bool _bSeparateDistributionManager) &&
	{
		m_bSeparateDistributionManager = _bSeparateDistributionManager;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_KeySetting(NCryptography::CPublicKeySetting _KeySetting) &&
	{
		m_KeySetting = _KeySetting;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_FriendlyName(CStr const &_FriendlyName) &&
	{
		m_FriendlyName = _FriendlyName;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_Enclave(CStr const &_Enclave) &&
	{
		m_Enclave = _Enclave;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_AuditCategory(NStr::CStr const &_Category) &&
	{
		m_AuditCategory = _Category;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RunAsUser(NStr::CStr const &_User) &&
	{
		m_RunAsUser = _User;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_RunAsGroup(NStr::CStr const &_Group) &&
	{
		m_RunAsGroup = _Group;
		return fg_Move(*this);
	}
	
	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_UpdateType(EDistributedAppUpdateType _UpdateType) &&
	{
		m_UpdateType = _UpdateType;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_InterfaceSettings(CDistributedAppActor_InterfaceSettings const &_InterfaceSettings) &&
	{
		m_InterfaceSettings = _InterfaceSettings;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_SupportUserAuthentication(bool _bSupportUserAuthentication) &&
	{
		m_bSupportUserAuthentication = _bSupportUserAuthentication;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_CanUseLocalListenAsPrimary(bool _bCanUseLocalListenAsPrimary) &&
	{
		m_bCanUseLocalListenAsPrimary = _bCanUseLocalListenAsPrimary;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_CommandLineBeforeAppStart(bool _bCommandLineBeforeAppStart) &&
	{
		m_bCommandLineBeforeAppStart = _bCommandLineBeforeAppStart;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_WaitForRemotes(bool _bWaitForRemotes) &&
	{
		m_bWaitForRemotes = _bWaitForRemotes;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_TimeoutForUnixSockets(bool _bTimeoutForUnixSockets) &&
	{
		m_bTimeoutForUnixSockets = _bTimeoutForUnixSockets;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_ReconnectDelay(fp64 _ReconnectDelay) &&
	{
		m_ReconnectDelay = _ReconnectDelay;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_HostTimeoutOnShutdown(fp64 _HostTimeoutOnShutdown) &&
	{
		m_HostTimeoutOnShutdown = _HostTimeoutOnShutdown;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_KillHostsTimeoutOnShutdown(fp64 _KillHostsTimeoutOnShutdown) &&
	{
		m_KillHostsTimeoutOnShutdown = _KillHostsTimeoutOnShutdown;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_MaxShutdownTime(fp64 _MaxShutdownTime) &&
	{
		m_MaxShutdownTime = _MaxShutdownTime;
		return fg_Move(*this);
	}

	CDistributedAppActor_Settings &&CDistributedAppActor_Settings::f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality _DefaultCommandLineFunctionality) &&
	{
		m_DefaultCommandLineFunctionality = _DefaultCommandLineFunctionality;
		return fg_Move(*this);
	}

	NStr::CStr CDistributedAppActor_Settings::f_GetCompositeFriendlyName() const
	{
		CStr FriendlyName = m_FriendlyName;
		if (FriendlyName.f_IsEmpty())
			FriendlyName = fg_Format("{}@{}", NProcess::NPlatform::fg_Process_GetUserName(), NProcess::NPlatform::fg_Process_GetComputerName()); 
		return fg_Format("{}/{}", FriendlyName, m_AppName);
	}
}
