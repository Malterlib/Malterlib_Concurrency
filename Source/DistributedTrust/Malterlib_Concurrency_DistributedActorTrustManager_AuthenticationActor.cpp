// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/RuntimeType>
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"

namespace NMib::NConcurrency
{
	using namespace NContainer;
	using namespace NStr;

	ICDistributedActorTrustManagerAuthenticationActor::ICDistributedActorTrustManagerAuthenticationActor() = default;
	ICDistributedActorTrustManagerAuthenticationActor::~ICDistributedActorTrustManagerAuthenticationActor() = default;

	TCMap<NStr::CStr, TCActor<ICDistributedActorTrustManagerAuthenticationActor>> ICDistributedActorTrustManagerAuthenticationActor::fs_GetRegisteredAuthenticationFactors
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		NMib::CRunTimeObjectInfo * const pAuthentificationFactors = fg_GetRuntimeTypeInfo("NMib::NConcurrency::ICDistributedActorTrustManagerAuthenticationActorFactory");
		TCMap<NStr::CStr, TCActor<ICDistributedActorTrustManagerAuthenticationActor>> Result;

		if (pAuthentificationFactors)
		{
			auto Iter = pAuthentificationFactors->m_Children.f_GetIter();

			while (Iter)
			{
				NMib::CRunTimeObjectInfo *pIter = Iter;
				NStr::CStr Name = pIter->f_GetName();
				if (Name.f_Find("CDistributedActorTrustManagerAuthenticationActorFactory") == 0)
				{
					Name = Name.f_Extract(fg_StrLen("CDistributedActorTrustManagerAuthenticationActorFactory"));
					if (auto *pFactory = (ICDistributedActorTrustManagerAuthenticationActorFactory *)pIter->f_CreateObject())
						Result[Name] = (*pFactory)(_TrustManager);
				}
				++Iter;
			}
		}
		return Result;
	}

	TCContinuation<TCSet<CStr>> CDistributedActorTrustManager::f_EnumAuthenticationFactors()
	{
		TCSet<CStr> Result;
		auto &Internal = *mp_pInternal;

		for (auto &Actor : Internal.m_AuthenticationActors)
			Result[Internal.m_AuthenticationActors.fs_GetKey(Actor)];

		return fg_Explicit(Result);
	}
}




