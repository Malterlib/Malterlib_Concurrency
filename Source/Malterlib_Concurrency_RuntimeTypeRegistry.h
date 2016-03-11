// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Intrusive/AVLTree>
#include <Mib/Core/SubSystemInterface>

#if defined(DMibIncluded_Exception) && !defined(DMibRuntimeTypeRegistry)
#	error "You need to include <Mib/Concurrency/RuntimeType> before execptions are included"
#endif

#define DMibRuntimeTypeRegistry

namespace NMib
{
	namespace NStream
	{
		class CBinaryStreamDefault;
		
		template <typename t_CStreamType>
		class CBinaryStreamMemoryPtr;
		
		template <typename t_CStreamType, typename t_CVector>
		class CBinaryStreamMemory;
	}
	namespace NException
	{
		class CExceptionBase;
	}
	namespace NConcurrency
	{
		template <typename t_CType>
		class TCAsyncResult;
		
		template <typename t_CReturnValue>
		struct TCContinuation;
	}
}

#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.h"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_MemberFunction.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CSubSystem_Concurrency_RuntimeTypeRegistry : public CSubSystem
		{
			CSubSystem_Concurrency_RuntimeTypeRegistry();
			~CSubSystem_Concurrency_RuntimeTypeRegistry();
			
			struct CSortMemberFunction_Hash
			{
				inline_always_debug uint32 operator ()(CRuntimeTypeRegistryEntry_MemberFunction const &_Entry);
			};

			struct CSort_MemberPointer
			{
				inline_always_debug CRuntimeTypeRegistry_MemberFunctionPointer const &operator ()(CRuntimeTypeRegistryEntry_MemberFunction const &_Entry);
			};

			struct CSortException_Hash
			{
				inline_always_debug uint32 operator ()(CRuntimeTypeRegistryEntry_Exception const &_Entry);
			};
			
			NIntrusive::TCAVLTree<CRuntimeTypeRegistryEntry_MemberFunction::CLinkTraits_m_HashLink, CSortMemberFunction_Hash> m_EntryByHash_MemberFunction;
			NIntrusive::TCAVLTree<CRuntimeTypeRegistryEntry_MemberFunction::CLinkTraits_m_MemberPointerLink, CSort_MemberPointer> m_EntryByMemberPointer;
			
			NIntrusive::TCAVLTree<CRuntimeTypeRegistryEntry_Exception::CLinkTraits_m_HashLink, CSortException_Hash> m_EntryByHash_Exception;
		};

		CSubSystem_Concurrency_RuntimeTypeRegistry &fg_RuntimeTypeRegistry();
	}
}

#ifndef DMibSafety_IncMalterlib_H
#	include "Malterlib_Concurrency_RuntimeTypeRegistry.hpp"
#endif
