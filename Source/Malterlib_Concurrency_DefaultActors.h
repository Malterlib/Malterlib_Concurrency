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
				mc_bImmediateDelete = true
			};

			void f_Call();
			void f_Dispatch(NFunction::TCFunction<void ()> && _ToDispatch);
		};		
		class CConcurrentActorLowPrio : public CConcurrentActor
		{
		public:
			enum
			{
				mc_Priority = EPriority_Low
			};
		};		
		
		class CAnyConcurrentActor : public CConcurrentActor
		{
		public:
			enum
			{
				mc_bCanBeEmpty = true
			};
		};
		
		TCActor<CAnyConcurrentActor> const &fg_AnyConcurrentActor();

		class CAnyConcurrentActorLowPrio : public CConcurrentActor
		{
		public:
			enum
			{
				mc_bCanBeEmpty = true
			};
		};
		
		TCActor<CAnyConcurrentActorLowPrio> const &fg_AnyConcurrentActorLowPrio();
		
	}
}
