// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
//#include <Mib/Core/RuntimeType>
//#include <Mib/Concurrency/DistributedActor>
//#include <Mib/Concurrency/DistributedApp>

namespace NMib::NConcurrency
{
	namespace NDistributedActorTrustManagerDatabase
	{
		struct CUserAuthenticationFactor;
	}

	enum EAuthenticationFactorCategory
	{
		EAuthenticationFactorCategory_None = 0
		, EAuthenticationFactorCategory_Possession
		, EAuthenticationFactorCategory_Inherence
		, EAuthenticationFactorCategory_Knowledge
	};

	NStr::CStr fg_AuthenticationFactorCategoryToString(EAuthenticationFactorCategory _Categories);

	struct CAuthenticationData
	{
		CAuthenticationData const &operator = (NDistributedActorTrustManagerDatabase::CUserAuthenticationFactor const &_Factor);

		EAuthenticationFactorCategory m_Category = EAuthenticationFactorCategory_None;
		NStr::CStr m_Name;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSONSorted> m_PublicData;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSONSorted> m_PrivateData;
	};

}
#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
