// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	struct CShamActorHolder;

	struct CConcurrentActor : public CActor
	{
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};
	struct CConcurrentActorImpl : public CConcurrentActor
	{
		~CConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	struct CConcurrentActorLowPrio : public CConcurrentActorImpl
	{
		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};
	struct CConcurrentActorLowPrioImpl : public CConcurrentActorLowPrio
	{
		~CConcurrentActorLowPrioImpl();
		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	namespace NPrivate
	{
		struct CDirectResultActor : public CActor
		{
			using CActorHolder = CShamActorHolder;

			static constexpr bool mc_bImmediateDelete = true;
			static constexpr bool mc_bIsAlwaysAlive = true;
		};

		struct CDirectResultActorImpl : public CDirectResultActor
		{
			~CDirectResultActorImpl();

			static constexpr bool mc_bIsAlwaysAlive = false;
		};

		TCActor<CDirectResultActor> const &fg_DirectResultActor();
	}

	struct CAnyConcurrentActor : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};

	struct CAnyConcurrentActorImpl : public CAnyConcurrentActor
	{
		~CAnyConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	TCActor<CAnyConcurrentActor> const &fg_AnyConcurrentActor();

	struct CAnyConcurrentActorLowPrio : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};

	struct CAnyConcurrentActorLowPrioImpl : public CAnyConcurrentActorLowPrio
	{
		~CAnyConcurrentActorLowPrioImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	TCActor<CAnyConcurrentActorLowPrio> const &fg_AnyConcurrentActorLowPrio();

	struct CDynamicConcurrentActor : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};

	struct CDynamicConcurrentActorImpl : public CDynamicConcurrentActor
	{
		~CDynamicConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	TCActor<CDynamicConcurrentActor> const &fg_DynamicConcurrentActor();

	struct CDynamicConcurrentActorLowPrio : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static TCActorInternal<CActor> *fs_GetRealActor(TCActorInternal<CActor> *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};

	struct CDynamicConcurrentActorLowPrioImpl : public CDynamicConcurrentActorLowPrio
	{
		~CDynamicConcurrentActorLowPrioImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	TCActor<CDynamicConcurrentActorLowPrio> const &fg_DynamicConcurrentActorLowPrio();

	class CDirectCallActorHolder;

	struct CDirectCallActor : public CActor
	{
		using CActorHolder = CDirectCallActorHolder;

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
	};

	struct CDirectCallActorImpl : public CDirectCallActor
	{
		~CDirectCallActorImpl();

		static constexpr bool mc_bIsAlwaysAlive = false;
	};

	TCActor<CDirectCallActor> const &fg_DirectCallActor();
}
