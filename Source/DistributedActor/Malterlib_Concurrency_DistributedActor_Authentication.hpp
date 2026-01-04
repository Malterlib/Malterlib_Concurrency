// Copyright © 2025 Unbroken AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

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

