// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedActor_AuthenticationHandler.h"

#include <Mib/Concurrency/DistributedActor>

namespace NMib::NConcurrency
{
	ICDistributedActorAuthenticationHandler::ICDistributedActorAuthenticationHandler()
	{
		DMibPublishActorFunction(ICDistributedActorAuthenticationHandler::f_AuthenticateCommand);
	}

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CPermissionWithRequirements::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Permission;
		_Stream % m_AuthenticationFactors;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CPermissionWithRequirements);

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CRequest::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Description;
		_Stream % m_RequestedPermissions;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CRequest);

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CChallenge::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_ChallengeData;
		_Stream % m_UserID;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CChallenge);

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CResponse::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Permissions;
		_Stream % m_Challenge;
		_Stream % m_FactorID;
		_Stream % m_FactorName;
		_Stream % m_ResponseData;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CResponse);
}
