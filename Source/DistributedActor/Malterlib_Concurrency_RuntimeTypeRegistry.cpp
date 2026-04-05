// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include <Mib/Core/Core>
#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		constinit TCSubSystem<CSubSystem_Concurrency_RuntimeTypeRegistry, ESubSystemDestruction_BeforeMemoryManager> g_MalterlibSubSystem_Concurrency_RuntimeTypeRegistry = {DAggregateInit};
	}

	CSubSystem_Concurrency_RuntimeTypeRegistry &fg_RuntimeTypeRegistry()
	{
		return *NPrivate::g_MalterlibSubSystem_Concurrency_RuntimeTypeRegistry;
	}

	CRuntimeTypeRegistryEntry_MemberFunction::CRuntimeTypeRegistryEntry_MemberFunction
		(
			uint32 _Hash
			, uint32 _TypeHash
			, uint32 _LowestSupportedVersion
		)
		: m_Hash(_Hash)
		, m_TypeHash(_TypeHash)
		, m_LowestSupportedVersion(_LowestSupportedVersion)
	{
		auto &SubSystem = fg_RuntimeTypeRegistry();
		// Check that we don't have any hash collisions
		DMibFastCheck(!SubSystem.m_EntryByHash_MemberFunction.f_FindEqual(_Hash));
		SubSystem.m_EntryByHash_MemberFunction.f_Insert(this);
	}

	CRuntimeTypeRegistryEntry_MemberFunction::~CRuntimeTypeRegistryEntry_MemberFunction()
	{
		auto &SubSystem = fg_RuntimeTypeRegistry();
		SubSystem.m_EntryByHash_MemberFunction.f_Remove(this);
	}

	CRuntimeTypeRegistryEntry_Exception::CRuntimeTypeRegistryEntry_Exception(uint32 _Hash)
		: m_Hash(_Hash)
	{
		auto &SubSystem = fg_RuntimeTypeRegistry();
		DMibFastCheck(!SubSystem.m_EntryByHash_Exception.f_FindEqual(_Hash));
		SubSystem.m_EntryByHash_Exception.f_Insert(this);
	}

	CRuntimeTypeRegistryEntry_Exception::~CRuntimeTypeRegistryEntry_Exception()
	{
		auto &SubSystem = fg_RuntimeTypeRegistry();
		SubSystem.m_EntryByHash_Exception.f_Remove(this);
	}

	CSubSystem_Concurrency_RuntimeTypeRegistry::CSubSystem_Concurrency_RuntimeTypeRegistry()
	{
	}

	CSubSystem_Concurrency_RuntimeTypeRegistry::~CSubSystem_Concurrency_RuntimeTypeRegistry()
	{
	}
}
