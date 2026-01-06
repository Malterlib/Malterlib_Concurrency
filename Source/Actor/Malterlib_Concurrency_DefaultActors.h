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
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CConcurrentActorImpl : public CConcurrentActor
	{
		~CConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	struct CConcurrentActorLowPrio : public CConcurrentActorImpl
	{
		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CConcurrentActorLowPrioImpl : public CConcurrentActorLowPrio
	{
		~CConcurrentActorLowPrioImpl();
		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	struct CConcurrentActorHighCPU : public CConcurrentActorImpl
	{
		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CConcurrentActorHighCPUImpl : public CConcurrentActorHighCPU
	{
		~CConcurrentActorHighCPUImpl();
		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	struct CBlockingActor : public CSeparateThreadActor
	{
	};

	struct CThisConcurrentActor : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CThisConcurrentActorImpl final : public CThisConcurrentActor
	{
		~CThisConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CThisConcurrentActor> const &fg_ThisConcurrentActor();

	struct CThisConcurrentActorLowPrio : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CThisConcurrentActorLowPrioImpl : public CThisConcurrentActorLowPrio
	{
		~CThisConcurrentActorLowPrioImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CThisConcurrentActorLowPrio> const &fg_ThisConcurrentActorLowPrio();

	struct CThisConcurrentActorHighCPU : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CThisConcurrentActorHighCPUImpl : public CThisConcurrentActorHighCPU
	{
		~CThisConcurrentActorHighCPUImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CThisConcurrentActorHighCPU> const &fg_ThisConcurrentActorHighCPU();

	struct COtherConcurrentActor : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct COtherConcurrentActorImpl : public COtherConcurrentActor
	{
		~COtherConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<COtherConcurrentActor> const &fg_OtherConcurrentActor();

	struct COtherConcurrentActorLowPrio : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_Low;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct COtherConcurrentActorLowPrioImpl : public COtherConcurrentActorLowPrio
	{
		~COtherConcurrentActorLowPrioImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<COtherConcurrentActorLowPrio> const &fg_OtherConcurrentActorLowPrio();

	struct COtherConcurrentActorHighCPU : public CConcurrentActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;
		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct COtherConcurrentActorHighCPUImpl : public COtherConcurrentActorHighCPU
	{
		~COtherConcurrentActorHighCPUImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<COtherConcurrentActorHighCPU> const &fg_OtherConcurrentActorHighCPU();

	struct CDynamicConcurrentActor : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CDynamicConcurrentActorImpl : public CDynamicConcurrentActor
	{
		~CDynamicConcurrentActorImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CDynamicConcurrentActor> const &fg_DynamicConcurrentActor();

	struct CDynamicConcurrentActorLowPrio : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CDynamicConcurrentActorLowPrioImpl : public CDynamicConcurrentActorLowPrio
	{
		~CDynamicConcurrentActorLowPrioImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CDynamicConcurrentActorLowPrio> const &fg_DynamicConcurrentActorLowPrio();

	struct CDynamicConcurrentActorHighCPU : public CActor
	{
		using CActorHolder = CShamActorHolder;

		static DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fs_GetRealActor(NConcurrency::CActorHolder *_pActorInternal);

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CDynamicConcurrentActorHighCPUImpl : public CDynamicConcurrentActorHighCPU
	{
		~CDynamicConcurrentActorHighCPUImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CDynamicConcurrentActorHighCPU> const &fg_DynamicConcurrentActorHighCPU();

	class CDirectCallActorHolder;

	struct CDirectCallActor : public CActor
	{
		using CActorHolder = CDirectCallActorHolder;

		static constexpr bool mc_bImmediateDelete = true;
		static constexpr bool mc_bIsAlwaysAlive = true;
		static constexpr bool mc_bIsAlwaysAliveImpl = true;
	};

	struct CDirectCallActorImpl final : public CDirectCallActor
	{
		~CDirectCallActorImpl();

		static constexpr bool mc_bIsAlwaysAliveImpl = false;
	};

	TCActor<CDirectCallActor> const &fg_DirectCallActor();
}
