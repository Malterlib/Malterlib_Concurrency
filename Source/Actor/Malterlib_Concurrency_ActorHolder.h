// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Memory/Allocators/Placement>
#include <Mib/Concurrency/RunQueue>

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturn>
	struct TCGetReturnType;

	template <typename t_CReturn, typename t_CPromiseParam>
	struct TCCallActorGetReturnType;

	template
	<
		bool tf_bDirectCall
		, EVirtualCall tf_VirtualCall
		, typename tf_CActor
		, typename tf_CFunctor
		, typename tf_CPromiseParam
		, typename ...tfp_CParams
	>
	auto fg_CallActorFuture
		(
			tf_CActor &&_Actor
			, tf_CFunctor const &_pFunctionPointer
			, tf_CPromiseParam &&_PromiseParam
			, tfp_CParams && ...p_Params
		)
		-> typename TCCallActorGetReturnType<typename TCGetMemberOrNonMemberFunctionPointerTraits<tf_CFunctor>::CType::CReturn, NTraits::TCDecay<tf_CPromiseParam>>::CType
	;
}

namespace NMib::NConcurrency
{
	struct CDistributedActorHostInfo
	{
		NStr::CStr m_UniqueHostID;
		NStr::CStr m_RealHostID;
		bool m_bAnonymous = false;
	};

	struct CConcurrencyThreadLocal;

	mark_nodebug CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal();

	struct CFutureCoroutineContext;

	struct ICDistributedActorData
	{
		virtual ~ICDistributedActorData();
		inline_always bool f_IsValidForCall() const
		{
			return !m_bRemote;
		}
		virtual CDistributedActorHostInfo f_GetHostInfo() const = 0;

		NStorage::CIntrusiveRefCountWithWeak m_RefCount;
		NStr::CStr m_ActorID;
		uint32 m_ProtocolVersion = TCLimitsInt<uint32>::mc_Max;
		bool m_bWasDestroyed = false;
		bool m_bRemote = false;
	};

	/// \brief Manages actor execution and lifetime
	class CActorHolder DWorkaroundVirtualLayout
	{
		friend class CConcurrencyManager;
		friend struct CActor;
		friend class CDelegatedActorHolder;
		friend struct CActorCommon;
		friend struct CFutureCoroutineContext;

		template <typename t_CImplementation>
		friend struct TCDistributedActorInstance;

		friend CFutureCoroutineContextOnResumeScopeAwaiter fg_CurrentActorCheckDestroyedOnResume(CConcurrencyThreadLocal &_ThreadLocal);

		struct CDestroyHandler;

		using FDestroyHolderOnDestroyed = NFunction::TCFunction
			<
				void (NFunction::CThisTag &)
				, NFunction::CFunctionNoCopyTag
				, NFunction::TCFunctionNoAllocOptions
				<
					false
#ifdef DPlatformFamily_Windows
					, sizeof(void *) * 3 // std::exception_ptr is two pointers on Windows
#else
					, sizeof(void *) * 2
#endif
					, sizeof(void *)
					, false
				>
			>
		;

	protected:
		bool fp_AddToQueue(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		bool fp_AddToQueue(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal);
		virtual void fp_StartQueueProcessing();

		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCActor<tf_CActor> fp_GetAsActor();

		virtual void fp_DestroyThreaded();
		virtual void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void fp_Yield();

		void fp_RunProcess(CConcurrencyThreadLocal &_ThreadLocal);
		virtual DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fp_GetRealActor(NConcurrency::CActorHolder *_pActorInternal) const = 0;
		static auto fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &&_Promise);

		void fp_DeleteActor();
		void fp_DetachActor();
		CActor *fp_GetActorRelaxed() const;

		static bool fsp_ScheduleActorDestroy(CActorHolder *_pActorHolder, CConcurrencyThreadLocal &_ThreadLocal);

	public:
		CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);
		virtual ~CActorHolder();

		void f_Yield();
		void f_SetFixedQueue(umint _iFixedCore);
		void f_QueueProcessDestroy(FActorQueueDispatch &&_Functor);
		void f_QueueProcess(FActorQueueDispatch &&_Functor);
		void f_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry);

		inline_always void f_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		inline_always void f_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		inline_always void f_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal);
		bool f_ImmediateDelete() const;
		bool f_IsAlwaysAlive() const;
		bool f_IsHolderDestroyed() const;
#if DMibEnableSafeCheck > 0
		bool f_CurrentlyProcessing() const;
#endif
		bool f_IsCurrentActorAndProcessing() const;

		void f_BlockDestroy(CActorDestroyEventLoop const &_EventLoop = CActorDestroyEventLoop());

		inline_always CConcurrencyManager &f_ConcurrencyManager() const;
		inline_always EPriority f_GetPriority() const;

		NStorage::TCSharedPointer<ICDistributedActorData> &f_GetDistributedActorData();
		NStorage::TCSharedPointer<ICDistributedActorData> const &f_GetDistributedActorData() const;
		CDistributedActorHostInfo f_GetHostInfo();

		static bool fs_ScheduleActorHolderDestroy(CActorHolder *_pActorHolder, bool &o_bImmediateDelete);

#if DMibConfig_Tests_Enable
		void f_TestDetach();
#endif

		NStorage::CIntrusiveRefCountWithWeak m_RefCount;

	private:
		void fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory);

		void fp_DestroyActorHolder
			(
				FDestroyHolderOnDestroyed &&_fOnDestroyed
				, TCActorHolderSharedPointer<CActorHolder> &&_pSelfReference
				, CConcurrencyThreadLocal &_ThreadLocal
			)
		;
		bool fp_Terminate();

		bool fp_DequeueProcess(CConcurrencyThreadLocal &_ThreadLocal);

	protected:
		struct COnTerminate
		{
			COnTerminate() = default;
			COnTerminate(COnTerminate const &) = delete;
			COnTerminate(COnTerminate &&) = default;

			NFunction::TCFunctionMutable<void ()> m_fOnTerminate;

			COrdering_Strong operator <=> (COnTerminate const &_Other) const noexcept
			{
				return this <=> &_Other;
			}
		};

		// Alignment zone 1 (Static data, set at init) = 8 + 8 * 4 + 4 + 1 = 46 => 64 bytes
		NStorage::TCSharedPointer<ICDistributedActorData> mp_pDistributedActorData;
		NContainer::TCSet<COnTerminate> mp_OnTerminate;
		CConcurrencyManager *mp_pConcurrencyManager;
		NAtomic::TCAtomic<CActor *> mp_pActorUnsafe{nullptr}; // Changed at destruction only once
		uint32 mp_iFixedQueue{gc_InvalidQueue};

		// Note: Changing these is not thread safe
		static_assert(EPriority_Max <= 4);
		uint8 mp_Priority:2 = 0;
		uint8 mp_bImmediateDelete:1 = false;
		uint8 mp_bIsAlwaysAlive:1 = false;
		uint8 mp_bHasOverriddenDestroy:1 = false;

		static constexpr umint mc_AligmentZone1Size = sizeof(void *) + sizeof(void *) * 4 + sizeof(uint32) + sizeof(uint8);

		uint8 mp_PaddingZone1[fg_AlignUpConstExpr(mc_AligmentZone1Size, DMibPMemoryCacheLineSize) - mc_AligmentZone1Size];

		// Alignment zone 2 (Changed from other threads) = 8 + 8 + 4 = 20 => 64 bytes
		CConcurrentRunQueue mp_ConcurrentRunQueue;
		NAtomic::TCAtomic<umint> mp_Working;

		NAtomic::TCAtomic<uint32> mp_iLastQueue{gc_InvalidQueue}; // Changed from local thread, read from other threads

	private:
#if DMibConfig_Concurrency_DebugBlockDestroy
		DMibListLinkDS_Link(CActorHolder, m_ActorLink);
	public:
		NStr::CStr m_ActorTypeName;
#endif
		DIfRefCountDebugging(NStorage::CRefCountDebugReference m_DebugSelfRef);

	protected:
		static constexpr umint mc_AligmentZone2Size = sizeof(void *) * 2 + sizeof(uint32)
#if DMibConfig_RefCountDebugging || DMibConfig_Concurrency_DebugBlockDestroy
			+ (fg_AlignUpConstExpr(sizeof(uint32), sizeof(void *)) - sizeof(uint32))
#endif
#if DMibConfig_Concurrency_DebugBlockDestroy
			+ sizeof(void *) * 3
#endif
#if DMibConfig_RefCountDebugging
			+ sizeof(void *)
#endif
		;

		uint8 mp_PaddingZone2[fg_AlignUpConstExpr(mc_AligmentZone2Size, DMibPMemoryCacheLineSize) - mc_AligmentZone2Size];

		// Alignment zone 3 (Only changed and read from local thread) = 8 * 2 + 1 * 2 = 18 => 64 bytes
		CConcurrentRunQueue::CLocalQueueData mp_ConcurrentRunQueueLocal;

		mutable NAtomic::TCAtomic<uint8> mp_Destroyed;
		mutable NAtomic::TCAtomic<uint8> mp_DestroyInitiated{0};

		bool mp_bYield = false;

		// Alignment zone 4: Actor storage, other actor holder data
	};

	class CDefaultActorHolder : public CActorHolder
	{
	public:
		CDefaultActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;
		~CDefaultActorHolder();

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;
	};

	class CDispatchingActorHolder : public CDefaultActorHolder
	{
	public:
		CDispatchingActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> &&_Dispatcher
			)
		;

	protected:
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;

		NFunction::TCFunctionMovable<void (FActorQueueDispatchNoAlloc &&_Dispatch)> mp_Dispatcher;
		NThread::CLowLevelLock mp_DestroyLock;
	};

	class CDelegatedActorHolder : public CDefaultActorHolder
	{
	public:
		CDelegatedActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, TCActor<> const &_DelegateToActor
			)
		;
		~CDelegatedActorHolder();

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_Yield() override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;

		TCActorHolderSharedPointer<CActorHolder> mp_pDelegateTo;
		COnTerminate const *mp_pOnTerminateEntry = nullptr;
	};

	class CSeparateThreadActorHolder : public CDefaultActorHolder
	{
	public:
		CSeparateThreadActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
				, NStr::CStr const &_ThreadName
			)
		;
		~CSeparateThreadActorHolder();

		NStr::CStr const &f_GetThreadName();

	protected:
		void fp_StartQueueProcessing() override;
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueJob(FActorQueueDispatchNoAlloc &&_ToQueue, CConcurrencyThreadLocal &_ThreadLocal);
		void fp_RunQueue(CConcurrencyThreadLocal &_ThreadLocal);

		CConcurrentRunQueueNonVirtualNoAlloc::CLocalQueueData mp_JobQueueLocal;
		NThread::CLowLevelLock mp_ThreadLock;
		NStorage::TCUniquePointer<NThread::CThreadObject> mp_pThread;
		NStr::CStr mp_ThreadName;

		align_cacheline CConcurrentRunQueueNonVirtualNoAlloc mp_JobQueue;
		NAtomic::TCAtomic<umint> mp_JobQueueWorking;
	};

	class CDirectCallActorHolder : public CDefaultActorHolder
	{
	public:
		CDirectCallActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;
		~CDirectCallActorHolder();

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;
	};

	struct CShamActorHolder : public CDefaultActorHolder
	{
		CShamActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;
		~CShamActorHolder();

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueRunProcess(CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcessEntry(CConcurrentRunQueueEntryHolder &&_Entry, CConcurrencyThreadLocal &_ThreadLocal) override;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
