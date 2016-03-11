// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <initializer_list>

#include "Malterlib_Concurrency_AsyncResult.h"
#include "Malterlib_Concurrency_Continuation.h"

namespace NMib
{
	namespace NConcurrency
	{
		uint32 CSubSystem_Concurrency_RuntimeTypeRegistry::CSortMemberFunction_Hash::operator ()(CRuntimeTypeRegistryEntry_MemberFunction const &_Entry)
		{
			return _Entry.m_Hash;
		}

		CRuntimeTypeRegistry_MemberFunctionPointer const &CSubSystem_Concurrency_RuntimeTypeRegistry::CSort_MemberPointer::operator ()(CRuntimeTypeRegistryEntry_MemberFunction const &_Entry)
		{
			return _Entry.m_MemberPointer;
		}

		uint32 CSubSystem_Concurrency_RuntimeTypeRegistry::CSortException_Hash::operator ()(CRuntimeTypeRegistryEntry_Exception const &_Entry)
		{
			return _Entry.m_Hash;
		}
	}
}
		
#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.hpp"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_MemberFunction.hpp"

