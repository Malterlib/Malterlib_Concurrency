// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStream>
	void ICDistributedActorAuthentication::CRegisterAuthenticationHandlerParams::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_Handler);
		_Stream % m_UserID;
		if (_Stream.f_GetVersion() >= EProtocolVersion_AddHandlerID)
			_Stream % m_HandlerID;
	}
}

