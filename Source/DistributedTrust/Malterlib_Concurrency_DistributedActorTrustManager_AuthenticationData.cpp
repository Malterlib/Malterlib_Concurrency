// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationData.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib::NConcurrency
{
	CAuthenticationData const &CAuthenticationData::operator = (NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor const &_Factor)
	{
		// Make sure you update the function below as well
		m_Category = _Factor.m_Category;
		m_Name = _Factor.m_Name;
		m_PublicData = _Factor.m_PublicData;
		m_PrivateData = _Factor.m_PrivateData;
		return *this;
	}

	NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor const &NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor::operator = (CAuthenticationData const &_Data)
	{
		// Make sure you update the function above as well
		m_Category = _Data.m_Category;
		m_Name = _Data.m_Name;
		m_PublicData = _Data.m_PublicData;
		m_PrivateData = _Data.m_PrivateData;
		return *this;
	}

	NStr::CStr fg_AuthenticationFactorCategoryToString(EAuthenticationFactorCategory _Category)
	{
		switch (_Category)
		{
			case EAuthenticationFactorCategory_Possession: return "Posession";
			case EAuthenticationFactorCategory_Inherence: return "Inherence";
			case EAuthenticationFactorCategory_Knowledge: return "Knowledge";
			case EAuthenticationFactorCategory_None: return "None";
			default: return "None";
		}
	}
}
