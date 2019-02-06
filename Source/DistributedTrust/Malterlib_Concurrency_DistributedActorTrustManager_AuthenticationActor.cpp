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

	TCMap<NStr::CStr, CAuthenticationActorInfo> ICDistributedActorTrustManagerAuthenticationActor::fs_GetRegisteredAuthenticationFactors
		(
			TCActor<CDistributedActorTrustManager> const &_TrustManager
		)
	{
		NMib::CRunTimeObjectInfo * const pAuthenticationFactors = fg_GetRuntimeTypeInfo("NMib::NConcurrency::ICDistributedActorTrustManagerAuthenticationActorFactory");
		TCMap<NStr::CStr, CAuthenticationActorInfo> Result;

		if (pAuthenticationFactors)
		{
			for (auto &RunTimeObjectInfo : pAuthenticationFactors->m_Children)
			{
				NStr::CStr Name = RunTimeObjectInfo.f_GetName();
				if (!Name.f_StartsWith("CDistributedActorTrustManagerAuthenticationActorFactory"))
					continue;

				Name = Name.f_Extract(fg_StrLen("CDistributedActorTrustManagerAuthenticationActorFactory"));
				if (auto *pFactory = (ICDistributedActorTrustManagerAuthenticationActorFactory *)RunTimeObjectInfo.f_CreateObject())
				{
					auto Cleanup = g_OnScopeExit > [&]
						{
							delete pFactory;
						}
					;
					Result[Name] = (*pFactory)(_TrustManager);
				}
			}
		}
		return Result;
	}

	TCFuture<TCMap<CStr, CAuthenticationActorInfo>> CDistributedActorTrustManager::f_EnumAuthenticationActors() const
	{
		auto &Internal = *mp_pInternal;
		return fg_Explicit(Internal.m_AuthenticationActors);
	}
}
