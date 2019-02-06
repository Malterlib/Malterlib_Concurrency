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
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		virtual TCFuture<TCActorSubscriptionWithID<>> f_RegisterAuthenticationHandler
			(
				TCDistributedActorInterfaceWithID<ICDistributedActorAuthenticationHandler> &&_Handler
				, NStr::CStr const &_UserID
			) = 0
		;
		virtual TCFuture<bool> f_AuthenticatePermissionPattern
			(
				NStr::CStr const &_Pattern
				, NContainer::TCSet<NStr::CStr> const &_AuthenticationFactors
			 	, NStr::CStr const &_RequestID
			) = 0
		;
	};
}

