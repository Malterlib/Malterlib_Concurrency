// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	template <>
	void fg_CopyReplyToContinuation
		(
		 	TCContinuation<void> &_Continuation
		 	, NContainer::CSecureByteVector const &_Data
		 	, CDistributedActorStreamContext &_Context, uint32 _Version
		)
	{
		NStream::CBinaryStreamMemoryPtr<> ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToContinuationOrAsyncResultShared(ReplyStream, _Continuation))
			return;
		NStr::CStr Error;
		if (!_Context.f_ValidateContext(Error))
		{
			_Continuation.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
			return;
		}
		_Continuation.f_SetResult();
	}
}
