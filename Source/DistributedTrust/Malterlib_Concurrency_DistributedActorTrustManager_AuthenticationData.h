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
		struct CAuthenticationFactor;
	}

	enum EAuthenticationFactorCategory
	{
		EAuthenticationFactorCategory_None = 0
		, EAuthenticationFactorCategory_Knowledge
		, EAuthenticationFactorCategory_Possession
		, EAuthenticationFactorCategory_Inherence
	};

	struct CAuthenticationData
	{
		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream);

		CAuthenticationData const &operator = (NDistributedActorTrustManagerDatabase::CAuthenticationFactor const &_Factor);

		EAuthenticationFactorCategory m_Category;
		NStr::CStr m_Name;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_PublicData;
		NContainer::TCMap<NStr::CStr, NEncoding::CEJSON> m_PrivateData;
	};

}
#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationData.hpp"

