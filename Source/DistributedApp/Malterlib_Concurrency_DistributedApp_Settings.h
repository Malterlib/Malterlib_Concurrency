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
		CDistributedAppActor_Settings() = default;
		CDistributedAppActor_Settings
			(
				NStr::CStr const &_AppName
				, bool _bRequireListen
				, NStr::CStr const &_RootDirectory = fg_DistributedAppThreadLocal().m_DefaultSettings.m_RootDirectory
				, bool _bSeparateDistributionManager = fg_DistributedAppThreadLocal().m_DefaultSettings.m_bSeparateDistributionManager
				, NNet::CSSLKeySetting _KeySetting = fg_DistributedAppThreadLocal().m_DefaultSettings.m_KeySetting
				, NStr::CStr const &_FriendlyName = fg_DistributedAppThreadLocal().m_DefaultSettings.m_FriendlyName
				, NStr::CStr const &_Enclave = fg_DistributedAppThreadLocal().m_DefaultSettings.m_Enclave
				, EDistributedAppUpdateType _UpdateType = fg_DistributedAppThreadLocal().m_DefaultSettings.m_UpdateType
				, NStr::CStr const &_AuditCategory = fg_DistributedAppThreadLocal().m_DefaultSettings.m_AuditCategory
			 	, CDistributedAppActor_InterfaceSettings const &_InterfaceSettings = fg_DistributedAppThreadLocal().m_DefaultSettings.m_InterfaceSettings
			 	, bool _bSupportUserAuthentication = fg_DistributedAppThreadLocal().m_DefaultSettings.m_bSupportUserAuthentication
				, NStr::CStr const &_RunAsUser = fg_DistributedAppThreadLocal().m_DefaultSettings.m_RunAsUser
			 	, NStr::CStr const &_RunAsGroup = fg_DistributedAppThreadLocal().m_DefaultSettings.m_RunAsGroup
			)
		;
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
		CDistributedAppActor_Settings &&f_KeySetting(NNet::CSSLKeySetting _KeySetting) &&;
		CDistributedAppActor_Settings &&f_UpdateType(EDistributedAppUpdateType _UpdateType) &&;
		CDistributedAppActor_Settings &&f_InterfaceSettings(CDistributedAppActor_InterfaceSettings const &_InterfaceSettings) &&;
		CDistributedAppActor_Settings &&f_SupportUserAuthentication(bool _bSupportUserAuthentication) &&;

	private:
		NStr::CStr fp_GetLocalSocketPath(NStr::CStr const &_Prefix, bool _bEnclaveSpecific, NStr::CStr const &_Enclave) const;
	};
}
