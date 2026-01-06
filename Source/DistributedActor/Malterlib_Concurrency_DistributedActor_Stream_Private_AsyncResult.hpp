// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CType>
	void CDistributedActorReadStream::f_Consume(TCAsyncResult<tf_CType> &_AsyncResult)
	{
		_AsyncResult.f_ConsumeWithProtocol(*this, NPrivate::fg_ActorProtocolVersion(f_GetContext()));
	}

	template <typename tf_CType>
	void CDistributedActorWriteStream::f_Feed(TCAsyncResult<tf_CType> const &_AsyncResult)
	{
		_AsyncResult.f_FeedWithProtocol(*this, NPrivate::fg_ActorProtocolVersion(f_GetContext()));
	}

	template <typename tf_CType>
	void CDistributedActorWriteStream::f_Feed(TCAsyncResult<tf_CType> &&_AsyncResult)
	{
		_AsyncResult.f_FeedWithProtocol(*this, NPrivate::fg_ActorProtocolVersion(f_GetContext()));
	}
}
