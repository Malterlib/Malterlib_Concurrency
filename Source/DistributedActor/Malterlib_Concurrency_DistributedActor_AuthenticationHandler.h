// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	struct ICDistributedActorAuthenticationHandler : public CActor
	{
		enum : uint32
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		ICDistributedActorAuthenticationHandler();

		struct CPermissionWithRequirements
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			NStr::CStr m_Permission;
			// This permission can be authenticated by any of the factor collections in the outer set. For example
			// {{ "Factor1", "Factor2"}, { "Factor1", "Factor3"}} can be authenticated by Factor1 and either Factor2 or Factor3
			NContainer::TCSet<NContainer::TCSet<NStr::CStr>> m_AuthenticationFactors;
		};

		struct CRequest
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			NStr::CStr m_Description;

			// An action can require multiple permissions of different kinds. For example, to run the command to enumerate secrets the host/user combo must have one or both of
			// SecretsManager/AllCommand or SecretsManager/Command/EnumerateSecrets. Furthermore, the host/user combo must have permission to read the enumerated secret, i.e. it must
			// have least one of SecretsManager/Read/SemanticID/sem/Tag/tag or SecretsManager/Read/NoSemanticID/NoTag. Since the permissions can be granted to user, host or the host/user
			// pair we can theoretically have different authentication factors for the same permission string. (When authenticating the command we will try to find the combination of
			// authentication factors the requires the least interactions with the user.)
			//
			// For the enum secrets case our vector of vectors could contain
			// {
			// 		{SecretsManager/AllCommand, SecretsManager/Command/EnumerateSecrets}
			// 		, {SecretsManager/Read/SemanticID/sem/Tag/tag, SecretsManager/Read/NoSemanticID/NoTag}
			// }
			//
			// To authenticate this request we need authentication for at least one permission in each of the inner vectors.

			NContainer::TCVector<NContainer::TCVector<CPermissionWithRequirements>> m_RequestedPermissions;
		};

		struct CChallenge
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;
			inline bool operator == (CChallenge const &_Other) const;

			NContainer::TCVector<uint8> m_ChallengeData;
			NStr::CStr m_UserID;
		};

		struct CResponse
		{
			template <typename tf_CStream>
			void f_Stream(tf_CStream &_Stream);
			template <typename tf_CStr>
			void f_Format(tf_CStr &o_Str) const;

			NContainer::TCSet<NStr::CStr> m_Permissions;
			CChallenge m_Challenge;
			NStr::CStr m_FactorID;
			NStr::CStr m_FactorName;
			NContainer::TCVector<uint8> m_ResponseData;
		};

		virtual TCContinuation<NContainer::TCVector<CResponse>> f_AuthenticateCommand
			(
				CRequest const &_Request
				, NStr::CStr const &_UserID
				, CChallenge const &_Challenge
			) = 0
		;
	};
}

#include "Malterlib_Concurrency_DistributedActor_AuthenticationHandler.hpp"
