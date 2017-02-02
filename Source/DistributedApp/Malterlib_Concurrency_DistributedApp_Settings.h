// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	enum EDistributedAppUpdateType
	{
		EDistributedAppUpdateType_Independent
		, EDistributedAppUpdateType_OneAtATime
		, EDistributedAppUpdateType_AllAtOnce
	};
	
	struct CDistributedAppActor_Settings
	{
		CDistributedAppActor_Settings() = default;
		CDistributedAppActor_Settings
			(
				NStr::CStr const &_AppName
				, bool _bRequireListen
				, NStr::CStr const &_ConfigDirectory = NFile::CFile::fs_GetProgramDirectory()
				, bool _bSeparateConcurrencyManager = false
				, NNet::CSSLKeySetting _KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting()
				, NStr::CStr const &_FriendlyName = NStr::CStr()
				, NStr::CStr const &_Enclave = fg_DistributedActorSuggestedEnclave()
				, EDistributedAppUpdateType _UpdateType = EDistributedAppUpdateType_Independent 
				, NStr::CStr const &_AuditCategory = {}
			)
		;
		NStr::CStr f_GetCompositeFriendlyName() const;
		NStr::CStr f_GetLocalSocketHostname(bool _bEnclaveSpecific) const;
		
		CDistributedAppActor_Settings &&f_ConfigDirectory(NStr::CStr const &_ConfigDirectory) &&;
		CDistributedAppActor_Settings &&f_FriendlyName(NStr::CStr const &_FriendlyName) &&;
		CDistributedAppActor_Settings &&f_Enclave(NStr::CStr const &_Enclave) &&;
		CDistributedAppActor_Settings &&f_AuditCategory(NStr::CStr const &_Category) &&;
		CDistributedAppActor_Settings &&f_SeparateConcurrencyManager(bool _bSeparateConcurrencyManager) &&;
		CDistributedAppActor_Settings &&f_KeySetting(NNet::CSSLKeySetting _KeySetting) &&;
		CDistributedAppActor_Settings &&f_UpdateType(EDistributedAppUpdateType _UpdateType) &&;
		
		NStr::CStr m_AppName;
		NStr::CStr m_ConfigDirectory;
		NStr::CStr m_FriendlyName;
		NStr::CStr m_AuditCategory;
		NStr::CStr m_Enclave;
		NNet::CSSLKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
		NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
		EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
		bool m_bRequireListen = false;
		bool m_bSeparateConcurrencyManager = false;
	private:
		NStr::CStr fp_GetLocalSocketPath(NStr::CStr const &_Prefix, bool _bEnclaveSpecific) const;
	};
}
