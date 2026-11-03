// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
#ifdef DMibCanDetectVirtualMemberFunctions_Constexpr
	template <EVirtualCall tf_Detected, EVirtualCall tf_Specified>
	consteval EVirtualCall fg_CheckVirtualCallDetection()
	{
		static_assert(tf_Specified == EVirtualCall::mc_Dynamic || tf_Detected == tf_Specified);

		return tf_Detected;
	}
#endif
}

namespace NMib::NConcurrency
{
	template <typename tf_FOnResult>
	inline TCDirectResultFunctor<tf_FOnResult> CDirectResult::operator / (tf_FOnResult &&_fOnResult)
	{
		return {.m_fOnResult = fg_Move(_fOnResult)};
	}

	constexpr CBindActorOptions::CBindActorOptions(EVirtualCall _VirtualCall)
		: m_VirtualCall(_VirtualCall)
	{
	}

	constexpr CBindActorOptions::CBindActorOptions(EActorCallType _CallType)
		: m_CallType(_CallType)
	{
	}

	constexpr CBindActorOptions::CBindActorOptions(EVirtualCall _VirtualCall, EActorCallType _CallType)
		: m_VirtualCall(_VirtualCall)
		, m_CallType(_CallType)
	{
	}

	constexpr CBindActorOptions::CBindActorOptions(EActorCallType _CallType, EVirtualCall _VirtualCall)
		: m_VirtualCall(_VirtualCall)
		, m_CallType(_CallType)
	{
	}
}
