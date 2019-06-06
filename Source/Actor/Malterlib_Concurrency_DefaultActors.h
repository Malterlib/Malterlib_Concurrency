// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CShamActorHolder;

	struct CConcurrentActor : public CActor
	{
		static constexpr bool mc_bImmediateDelete = true;
	};

	struct CConcurrentActorLowPrio : public CConcurrentActor
	{
		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
	};

	namespace NPrivate
	{
		struct CDirectResultActor : public CActor
		{
			using CActorHolder = CShamActorHolder;

			static constexpr bool mc_bImmediateDelete = true;
		};

		TCActor<CDirectResultActor> const &fg_DirectResultActor();
	}

	struct CAnyConcurrentActor : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
	};

	TCActor<CAnyConcurrentActor> const &fg_AnyConcurrentActor();

	struct CAnyConcurrentActorLowPrio : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
	};

	TCActor<CAnyConcurrentActorLowPrio> const &fg_AnyConcurrentActorLowPrio();

	struct CDynamicConcurrentActor : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
	};

	TCActor<CDynamicConcurrentActor> const &fg_DynamicConcurrentActor();

	struct CDynamicConcurrentActorLowPrio : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
	};

	TCActor<CDynamicConcurrentActorLowPrio> const &fg_DynamicConcurrentActorLowPrio();

	class CDirectCallActorHolder;

	struct CDirectCallActor : public CActor
	{
		using CActorHolder = CDirectCallActorHolder;

		static constexpr bool mc_bImmediateDelete = true;
	};

	TCActor<CDirectCallActor> const &fg_DirectCallActor();
}
