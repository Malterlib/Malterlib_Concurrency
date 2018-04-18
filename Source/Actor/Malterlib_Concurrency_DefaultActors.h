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
		};
		
		class CConcurrentActorLowPrio : public CConcurrentActor
		{
		public:
			enum
			{
				mc_Priority = EPriority_Low
			};
		};		

		namespace NPrivate
		{
			class CDirectResultActor : public CActor
			{
			public:
			};
			
			TCActor<CDirectResultActor> const &fg_DirectResultActor();
		}
		
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

		class CDirectCallActorHolder;

		class CDirectCallActor : public CActor
		{
		public:
			using CActorHolder = CDirectCallActorHolder;
			enum
			{
				mc_bImmediateDelete = true
			};
		};

		TCActor<CDirectCallActor> const &fg_DirectCallActor();
	}
}
