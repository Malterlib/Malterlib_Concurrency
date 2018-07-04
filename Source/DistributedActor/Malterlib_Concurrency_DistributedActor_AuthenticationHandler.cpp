// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/DistributedActor>

namespace NMib::NConcurrency
{
	ICDistributedActorAuthenticationHandler::ICDistributedActorAuthenticationHandler()
	{
		DMibPublishActorFunction(ICDistributedActorAuthenticationHandler::f_RequestAuthentication);
		DMibPublishActorFunction(ICDistributedActorAuthenticationHandler::f_GetMultipleRequestSubscription);
	}

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CPermissionWithRequirements::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Permission;
		_Stream % m_AuthenticationFactors;
		_Stream % m_MaximumAuthenticationLifetime;
		_Stream % m_Preauthenticated;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CPermissionWithRequirements);

	bool ICDistributedActorAuthenticationHandler::CPermissionWithRequirements::operator < (CPermissionWithRequirements const &_Right) const
	{
		return fg_TupleReferences(m_Permission, m_AuthenticationFactors, m_MaximumAuthenticationLifetime, m_Preauthenticated)
			< fg_TupleReferences(_Right.m_Permission, _Right.m_AuthenticationFactors, _Right.m_MaximumAuthenticationLifetime, _Right.m_Preauthenticated)
		;
	}

	bool ICDistributedActorAuthenticationHandler::CPermissionWithRequirements::operator == (CPermissionWithRequirements const &_Right) const
	{
		return fg_TupleReferences(m_Permission, m_AuthenticationFactors, m_MaximumAuthenticationLifetime, m_Preauthenticated)
			== fg_TupleReferences(_Right.m_Permission, _Right.m_AuthenticationFactors, _Right.m_MaximumAuthenticationLifetime, _Right.m_Preauthenticated)
		;
	}

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CRequest::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Description;
		_Stream % m_RequestedPermissions;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CRequest);

	bool ICDistributedActorAuthenticationHandler::CRequest::operator < (CRequest const &_Right) const
	{
		return fg_TupleReferences(m_Description, m_RequestedPermissions) < fg_TupleReferences(_Right.m_Description, _Right.m_RequestedPermissions);
	}

	bool ICDistributedActorAuthenticationHandler::CRequest::operator == (CRequest const &_Right) const
	{
		return fg_TupleReferences(m_Description, m_RequestedPermissions) == fg_TupleReferences(_Right.m_Description, _Right.m_RequestedPermissions);
	}

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
		_Stream % m_ExpirationTime;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CResponse);

	template <typename tf_CStream>
	void ICDistributedActorAuthenticationHandler::CMultipleRequestData::f_Stream(tf_CStream &_Stream)
	{
		_Stream % fg_Move(m_Subscription);
		_Stream % m_ID;
	}
	DMibDistributedStreamImplement(ICDistributedActorAuthenticationHandler::CMultipleRequestData);
}
