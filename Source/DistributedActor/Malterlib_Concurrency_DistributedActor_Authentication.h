// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib::NConcurrency
{
	struct ICDistributedActorAuthentication : public CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/DistributedActorAuthentication";

		enum : uint32
		{
			EProtocolVersion_Min = 0x101
			, EProtocolVersion_Current = 0x101
		};

		virtual TCFuture<TCActorSubscriptionWithID<>> f_RegisterAuthenticationHandler
			(
				TCDistributedActorInterfaceWithID<ICDistributedActorAuthenticationHandler> _Handler
				, NStr::CStr _UserID
			) = 0
		;
		virtual TCFuture<bool> f_AuthenticatePermissionPattern
			(
				NStr::CStr _Pattern
				, NContainer::TCSet<NStr::CStr> _AuthenticationFactors
				, NStr::CStr _RequestID
			) = 0
		;
	};
}

