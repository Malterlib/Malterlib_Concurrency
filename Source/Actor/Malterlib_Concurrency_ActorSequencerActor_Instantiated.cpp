// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Malterlib_Concurrency_ActorSequencerActor.h"

namespace NMib::NConcurrency
{
	template struct TCActorSequencerActor<void>;
	template struct TCSequencer<void>;
}
