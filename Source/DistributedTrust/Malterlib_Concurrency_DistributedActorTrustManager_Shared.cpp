// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/DistributedActor>
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NContainer;

	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address() = default;
	CDistributedActorTrustManager_Address::~CDistributedActorTrustManager_Address() = default;
	
	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address(NHTTP::CURL const &_URL)
		: m_URL{_URL}
	{
	}
	
	CDistributedActorTrustManager_Address::CDistributedActorTrustManager_Address(NHTTP::CURL &&_URL)
		: m_URL{fg_Move(_URL)}
	{
	}

	bool CPermissionIdentifier::operator < (CPermissionIdentifier const &_Right) const
	{
		if (m_Type < _Right.m_Type)
			return true;
		if (m_Type > _Right.m_Type)
			return false;
		return m_ID < _Right.m_ID;
	}

	bool CPermissionIdentifier::operator == (CPermissionIdentifier const &_Right) const
	{
		return m_Type == _Right.m_Type && m_ID == _Right.m_ID;
	}

	CPermissionIdentifiers::CPermissionIdentifiers() = default;
	
	CPermissionIdentifiers::CPermissionIdentifiers(NStr::CStr const &_HostID, NStr::CStr const &_UserID)
	{
		if (_HostID)
			m_IDs[CPermissionIdentifier{EPermissionIdentifierType_HostID, _HostID}];
		if (_UserID)
			m_IDs[CPermissionIdentifier{EPermissionIdentifierType_UserID, _UserID}];
	}

	CPermissionIdentifiers CPermissionIdentifiers::fs_GetKeyFromFileNameOld(NStr::CStr const &_FileName)
	{
		if (!CActorDistributionManager::fs_IsValidHostID(_FileName))
			DMibError("Internal error: Malformed file name (host id)");

		CPermissionIdentifiers Key;
		Key.m_IDs[CPermissionIdentifier{EPermissionIdentifierType_HostID, _FileName}];
		return Key;
	}

	CPermissionIdentifiers CPermissionIdentifiers::fs_GetKeyFromFileName(NStr::CStr const &_FileName)
	{
		CPermissionIdentifiers Key;

		for (auto &IDString : _FileName.f_Split("-"))
		{
			CPermissionIdentifier Identifier;

			CFStr16 TypeString;
			{
				aint nParsed = 0;
				(CStr::CParse("{}_{}") >> TypeString >> Identifier.m_ID).f_Parse(IDString, nParsed);
				if (nParsed != 2)
					DMibError("Internal error: Malformed file name (type parse)");
			}

			if (TypeString == "H")
			{
				Identifier.m_Type = EPermissionIdentifierType_HostID;
				if (!CActorDistributionManager::fs_IsValidHostID(Identifier.m_ID))
					DMibError("Internal error: Malformed file name (host id)");
			}
			else if (TypeString == "U")
			{
				Identifier.m_Type = EPermissionIdentifierType_UserID;
				if (!CActorDistributionManager::fs_IsValidUserID(Identifier.m_ID))
					DMibError("Internal error: Malformed file name (user id)");
			}
			else
				DMibError("Internal error: Malformed file name (invalid type)");

			if (!Key.m_IDs(Identifier).f_WasCreated())
				DMibError("Internal error: Malformed file name (duplicate type)");
		}

		return Key;
	}

	TCVector<CPermissionIdentifiers> CPermissionIdentifiers::fs_GetPowerSet(NStr::CStr const &_HostID, NStr::CStr const &_UserID)
	{
		TCVector<CPermissionIdentifiers> Result;
		if (_HostID)
			Result.f_Insert(CPermissionIdentifiers(_HostID, ""));
		if (_UserID)
			Result.f_Insert(CPermissionIdentifiers("", _UserID));
		if (_HostID && _UserID)
			Result.f_Insert(CPermissionIdentifiers(_HostID, _UserID));
		return Result;
	}

	TCVector<CPermissionIdentifiers> CPermissionIdentifiers::fs_GetPowerSet(CPermissionIdentifiers const &_Identify)
	{
		TCVector<CPermissionIdentifiers> Result{_Identify};
		auto HostID = _Identify.f_GetHostID();
		auto UserID = _Identify.f_GetUserID();
		if (HostID != ms_EmptyID && UserID != ms_EmptyID)
		{
			Result.f_Insert(CPermissionIdentifiers(HostID, ""));
			Result.f_Insert(CPermissionIdentifiers("", UserID));
		}
		return Result;
	}

	NStr::CStr CPermissionIdentifiers::f_GetStr() const
	{
		NStr::CStr FileName;
		
		for (auto const &ID : m_IDs)
		{
			ch8 const *pIDType;
			switch (ID.m_Type)
			{
			case EPermissionIdentifierType_HostID:
				pIDType = "H";
				break;
			case EPermissionIdentifierType_UserID:
				pIDType = "U";
				break;

			default:
				DMibError("Internal error: Unhandled permission identifier type");
			}
			fg_AddStrSep(FileName, "{}_{}"_f << pIDType << ID.m_ID, "-");
		}
		return FileName;
	}
	
	NStr::CStr const CPermissionIdentifiers::ms_EmptyID;
	
	NStr::CStr const &CPermissionIdentifiers::f_GetHostID() const
	{
		for (auto const &ID : m_IDs)
		{
			if (ID.m_Type == EPermissionIdentifierType_HostID)
				return ID.m_ID;
		}
		return ms_EmptyID;
	}

	NStr::CStr const &CPermissionIdentifiers::f_GetUserID() const
	{
		for (auto const &ID : m_IDs)
		{
			if (ID.m_Type == EPermissionIdentifierType_UserID)
				return ID.m_ID;
		}
		return ms_EmptyID;
	}

	bool CPermissionIdentifiers::operator < (CPermissionIdentifiers const &_Right) const
	{
		return m_IDs < _Right.m_IDs;
	}

	bool CPermissionIdentifiers::operator == (CPermissionIdentifiers const &_Right) const
	{
		return m_IDs == _Right.m_IDs;
	}

	bool CPermissionRequirements::operator == (CPermissionRequirements const &_Right) const
	{
		return m_AuthenticationFactors == _Right.m_AuthenticationFactors && m_MaximumAuthenticationLifetime == _Right.m_MaximumAuthenticationLifetime;
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
