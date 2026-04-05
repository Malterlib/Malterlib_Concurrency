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
			, NContainer::CIOByteVector const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	{
		NStream::CBinaryStreamMemoryPtr<> ReplyStream;
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
