// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActor_Internal.h"

namespace NMib::NConcurrency::NPrivate
{
	template <>
	void fg_CopyReplyToPromise
		(
			TCPromise<void> &_Promise
			, NContainer::CSecureByteVector const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	{
		NStream::CBinaryStreamMemoryPtr<> ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToPromiseOrAsyncResultShared(ReplyStream, _Promise, _Context.f_ActorProtocolVersion()))
			return;
		NStr::CStr Error;
		if (!_Context.f_ValidateContext(Error))
		{
			_Promise.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
			return;
		}
		_Promise.f_SetResult();
	}
}
