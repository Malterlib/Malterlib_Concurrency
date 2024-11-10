// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Intrusive/AVLTree>
#include <Mib/Core/SubSystemInterface>
#include <Mib/Core/CoroutineFlags>

#if defined(DMibIncluded_Exception) && !defined(DMibRuntimeTypeRegistry)
#	error "You need to include <Mib/Concurrency/RuntimeType> before execptions are included"
#endif

#define DMibRuntimeTypeRegistry

namespace NMib::NStream
{
	class CBinaryStreamDefault;

	template <typename t_CStreamType>
	class CBinaryStreamMemoryPtr;

	template <typename t_CStreamType, typename t_CVector>
	class CBinaryStreamMemory;

	template <typename t_CStreamType>
	class TCBinaryStreamNull;
}

namespace NMib::NException
{
	class CExceptionBase;
}

namespace NMib::NConcurrency
{
	class CAsyncResult;

	template <typename t_CType>
	class TCAsyncResult;

	template <typename t_CReturnValue>
	struct TCPromise;

	template <typename t_CReturnValue>
	struct TCFuture;

	template <typename t_CReturnValue, ECoroutineFlag t_Flags>
	struct TCFutureWithFlags;

	template <typename t_CReturnValue = void>
	using TCUnsafeFuture = TCFutureWithFlags<t_CReturnValue, ECoroutineFlag_AllowReferences>;
}

#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.h"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_MemberFunction.h"

namespace NMib::NConcurrency
{
	struct CSubSystem_Concurrency_RuntimeTypeRegistry : public CSubSystem
	{
		CSubSystem_Concurrency_RuntimeTypeRegistry();
		~CSubSystem_Concurrency_RuntimeTypeRegistry();

		struct CSortMemberFunction_Hash
		{
			inline_always_debug uint32 operator ()(CRuntimeTypeRegistryEntry_MemberFunction const &_Entry);
		};

		struct CSortException_Hash
		{
			inline_always_debug uint32 operator ()(CRuntimeTypeRegistryEntry_Exception const &_Entry);
		};

		NIntrusive::TCAVLTree<&CRuntimeTypeRegistryEntry_MemberFunction::m_HashLink, CSortMemberFunction_Hash> m_EntryByHash_MemberFunction;

		NIntrusive::TCAVLTree<&CRuntimeTypeRegistryEntry_Exception::m_HashLink, CSortException_Hash> m_EntryByHash_Exception;
	};

	CSubSystem_Concurrency_RuntimeTypeRegistry &fg_RuntimeTypeRegistry();
}

#ifndef DMibSafety_IncMalterlib_H
#	include "Malterlib_Concurrency_RuntimeTypeRegistry.hpp"
#endif
