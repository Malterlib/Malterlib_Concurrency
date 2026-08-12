// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	template <>
	void fg_CopyReplyToAsyncResult
		(
			TCAsyncResult<void> &_AsyncResult
			, NStream::CBinaryStorage const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	{
		CDistributedActorReadStream ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToAsyncResultShared(ReplyStream, _AsyncResult, _Context.f_ActorProtocolVersion()))
			return;
		NStr::CStr Error;
		if (!_Context.f_ValidateContext(Error))
		{
			_AsyncResult.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
			return;
		}
		_AsyncResult.f_SetResult();
	}
}

namespace NMib::NConcurrency
{
	void CDistributedActorReadStream::f_OpenRead(NContainer::CSharedByteVector const &_View)
	{
		m_pBackingStorage.f_Clear();
		m_BackingStorage.f_Clear();
		m_BackingStorage.f_AppendShared(NContainer::CSharedByteVector(_View));
		NStream::TCBinaryStreamStoragePtr<>::f_OpenRead(m_BackingStorage);
	}

	void CDistributedActorReadStream::f_OpenRead(NStorage::TCSharedPointer<NStream::CBinaryStorage const> _pStorage, umint _Offset, umint _nBytes)
	{
		m_BackingStorage.f_Clear();
		m_pBackingStorage = fg_Move(_pStorage);
		NStream::TCBinaryStreamStoragePtr<>::f_OpenRead(*m_pBackingStorage, _Offset, _nBytes);
	}
}
