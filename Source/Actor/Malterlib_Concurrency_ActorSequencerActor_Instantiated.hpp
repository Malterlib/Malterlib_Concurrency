// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	extern template struct TCActorSequencerActor<void>;
	extern template struct TCSequencer<void>;
}
