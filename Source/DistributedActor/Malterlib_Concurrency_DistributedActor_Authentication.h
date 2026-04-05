// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

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
			, EProtocolVersion_AddHandlerID = 0x102
			, EProtocolVersion_Current = EProtocolVersion_AddHandlerID
		};

		struct CRegisterAuthenticationHandlerParams
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);

			TCDistributedActorInterfaceWithID<ICDistributedActorAuthenticationHandler> m_Handler;
			NStr::CStr m_UserID;
			uint32 m_HandlerID = 0;
		};

		virtual TCFuture<TCActorSubscriptionWithID<>> f_RegisterAuthenticationHandler
			(
				CRegisterAuthenticationHandlerParams _Params
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

#include "Malterlib_Concurrency_DistributedActor_Authentication.hpp"
