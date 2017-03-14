// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib::NConcurrency
{
	struct CLogAggregator : public CActor
	{
		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/Concurrency/LogAggregator";

		enum 
		{
			EMinProtocolVersion = 0x101
			, EProtocolVersion = 0x101
		};

		CLogAggregator();
		~CLogAggregator();
	};
}
