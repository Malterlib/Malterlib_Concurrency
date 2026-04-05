// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	void CDistributedActorWriteStream::f_Feed(TCActor<tf_CActor> const &_Actor)
	{
		this->f_FeedActor(_Actor);
	}

	template <typename tf_CActor>
	void CDistributedActorWriteStream::f_Feed(TCActor<tf_CActor> &&_Actor)
	{
		this->f_FeedActor(_Actor);
	}

	template <typename tf_CActor>
	void CDistributedActorReadStream::f_Consume(TCActor<tf_CActor> &_Actor)
	{
		this->f_ConsumeActor(_Actor, TCLimitsInt<uint32>::mc_Max);
	}
}
