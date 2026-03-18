// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CStr>
	void ICDistributedActorAuthenticationHandler::CPermissionWithRequirements::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{}{}:{vs} Lifetime {} Preauthenticated {vs}")
			<< m_Permission
			<< (!m_Description.f_IsEmpty() ? (tf_CStr::CFormat(" ({})") << m_Description).f_GetStr() : "")
			<< m_AuthenticationFactors
			<< m_MaximumAuthenticationLifetime
			<< m_Preauthenticated
		;
	}
	template <typename tf_CStr>
	void ICDistributedActorAuthenticationHandler::CRequest::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{} : {vs}") << m_Description << m_RequestedPermissions;
	}


	template <typename tf_CStr>
	void ICDistributedActorAuthenticationHandler::CChallenge::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{vs} {}") << m_ChallengeData << m_UserID;
	}

	bool ICDistributedActorAuthenticationHandler::CChallenge::operator == (CChallenge const &_Other) const noexcept
	{
		return m_ChallengeData == _Other.m_ChallengeData && m_UserID == _Other.m_UserID;
	}

	template <typename tf_CStr>
	void ICDistributedActorAuthenticationHandler::CResponse::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("{} ({}): {} {} {vs}") << m_FactorID << m_FactorName << m_SignedProperties.m_Permissions << m_SignedProperties.m_Challenges << m_Signature;
	}
}

