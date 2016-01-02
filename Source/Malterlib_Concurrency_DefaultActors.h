// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		class CConcurrentActor : public CActor
		{
		public:
			enum
			{
				mc_bConcurrent = true
				, mc_bImmediateDelete = true
			};

			void f_Call();
			void f_Dispatch(NFunction::TCFunction<void ()> && _ToDispatch);
		};		
	}
}
