// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include "Malterlib_Concurrency_DistributedApp_SettingsProperties.h"
#include "Malterlib_Concurrency_DistributedApp_ThreadLocal.h"

namespace NMib::NConcurrency
{
	struct CDistributedAppActor_Settings : public CDistributedAppActor_SettingsProperties
	{
		CDistributedAppActor_Settings();
		CDistributedAppActor_Settings(NStr::CStr const &_AppName);

		static CDistributedAppActor_SettingsProperties &fs_GetGlobalDefaultSettings();

		NStr::CStr f_GetCompositeFriendlyName() const;
		NStr::CStr f_GetLocalSocketHostname(bool _bEnclaveSpecific) const;
		NStr::CStr f_GetLocalSocketFileName(bool _bEnclaveSpecific, NStr::CStr const &_Enclave) const;
		NStr::CStr f_GetLocalSocketWildcard(bool _bEnclaveSpecific) const;
		
		CDistributedAppActor_Settings &&f_RootDirectory(NStr::CStr const &_RootDirectory) &&;
		CDistributedAppActor_Settings &&f_FriendlyName(NStr::CStr const &_FriendlyName) &&;
		CDistributedAppActor_Settings &&f_Enclave(NStr::CStr const &_Enclave) &&;
		CDistributedAppActor_Settings &&f_AuditCategory(NStr::CStr const &_Category) &&;
		CDistributedAppActor_Settings &&f_RunAsUser(NStr::CStr const &_User) &&;
		CDistributedAppActor_Settings &&f_RunAsGroup(NStr::CStr const &_Group) &&;
		CDistributedAppActor_Settings &&f_SeparateDistributionManager(bool _bSeparateDistributionManager) &&;
		CDistributedAppActor_Settings &&f_KeySetting(NCryptography::CPublicKeySetting _KeySetting) &&;
		CDistributedAppActor_Settings &&f_UpdateType(EDistributedAppUpdateType _UpdateType) &&;
		CDistributedAppActor_Settings &&f_InterfaceSettings(CDistributedAppActor_InterfaceSettings const &_InterfaceSettings) &&;
		CDistributedAppActor_Settings &&f_SupportUserAuthentication(bool _bSupportUserAuthentication) &&;
		CDistributedAppActor_Settings &&f_CanUseLocalListenAsPrimary(bool _bCanUseLocalListenAsPrimary) &&;
		CDistributedAppActor_Settings &&f_WaitForRemotes(bool _bWaitForRemotes) &&;
		CDistributedAppActor_Settings &&f_DefaultCommandLineFunctionalies(EDefaultCommandLineFunctionality _DefaultCommandLineFunctionality) &&;
		CDistributedAppActor_Settings &&f_CommandLineBeforeAppStart(bool _bCommandLineBeforeAppStart) &&;
		CDistributedAppActor_Settings &&f_TimeoutForUnixSockets(bool _bTimeoutForUnixSockets) &&;
		CDistributedAppActor_Settings &&f_ReconnectDelay(fp64 _ReconnectDelay) &&;
		CDistributedAppActor_Settings &&f_HostTimeoutOnShutdown(fp64 _HostTimeoutOnShutdown) &&;
		CDistributedAppActor_Settings &&f_KillHostsTimeoutOnShutdown(fp64 _HostTimeoutOnShutdown) &&;
		CDistributedAppActor_Settings &&f_MaxShutdownTime(fp64 _MaxShutdownTime) &&;
	private:
		NStr::CStr fp_GetLocalSocketPath(NStr::CStr const &_Prefix, bool _bEnclaveSpecific, NStr::CStr const &_Enclave) const;
	};
}
