// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Memory/Allocators/Placement>

namespace NMib::NConcurrency
{
	constexpr static const mint gc_ActorQueueDispatchFunctionMemory = fg_AlignUpConstExpr(sizeof(void*)*4*2, mint(DMibPMemoryCacheLineSize));

	using FActorQueueDispatch = NFunction::TCFunction
		<
			void (NFunction::CThisTag &)
			, NFunction::CFunctionNoCopyTag
			, NFunction::TCFunctionNoAllocOptions
			<
				true
				, gc_ActorQueueDispatchFunctionMemory - fg_AlignUpConstExpr(sizeof(void *)*(2 + 1), NFunction::TCFunctionNoAllocOptions<>::mc_Alignment)
				, NFunction::TCFunctionNoAllocOptions<>::mc_Alignment
				, false
			>
		>
	;

	struct CConcurrentRunQueue
	{
		struct align_cacheline CQueueEntry final
		{
			union
			{
				NAtomic::TCAtomic<CQueueEntry *> m_pNextQueued;
				DMibListLinkDSA_LinkType m_Link;
			};
			DMibListLinkD_Trans(CQueueEntry, m_Link);
			FActorQueueDispatch m_fToCall;
			CQueueEntry(FActorQueueDispatch &&_fToCall);
			~CQueueEntry();
		};

		static_assert(sizeof(CQueueEntry) <= gc_ActorQueueDispatchFunctionMemory);

		struct CLocalQueueData
		{
			~CLocalQueueData();

			DMibListLinkDS_List(CQueueEntry, m_Link) m_LocalQueue;
		};

		CConcurrentRunQueue();
		~CConcurrentRunQueue();
		void f_AddToQueue(FActorQueueDispatch &&_Functor);
		bool f_AddToQueueLocal(FActorQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue);
		bool f_AddToQueueLocalFirst(FActorQueueDispatch &&_Functor, CLocalQueueData &_LocalQueue);
		FActorQueueDispatch *f_FirstQueueEntry(CLocalQueueData &_LocalQueue);
		void f_PopQueueEntry(FActorQueueDispatch *_pEntry);
		bool f_TransferThreadSafeQueue(CLocalQueueData &_LocalQueue);
		bool f_IsEmpty(CLocalQueueData &_LocalQueue);
		bool f_OneOrLessInQueue(CLocalQueueData &_LocalQueue);
		void f_RemoveAll(CLocalQueueData &_LocalQueue);
		void f_RemoveAllExceptFirst(CLocalQueueData &_LocalQueue);

	private:
		NAtomic::TCAtomic<CQueueEntry *> mp_pFirstQueued;
	};

	struct CDistributedActorHostInfo
	{
		NStr::CStr m_UniqueHostID;
		NStr::CStr m_RealHostID;
		bool m_bAnonymous = false;
	};

	struct CConcurrencyThreadLocal;
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

		struct CDestroyHandler;

	protected:
		bool fp_AddToQueue(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal);
		virtual void fp_StartQueueProcessing();

		template <typename tf_CActor>
		DMibSuppressUndefinedSanitizer TCActor<tf_CActor> fp_GetAsActor();

		virtual void fp_DestroyThreaded();
		virtual void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) = 0;
		virtual void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) = 0;
		void fp_RunProcess();
		virtual DMibSuppressUndefinedSanitizer TCActorInternal<CActor> *fp_GetRealActor(NConcurrency::CActorHolder *_pActorInternal) const = 0;
		static auto fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &_Promise);

		void fp_DeleteActor();
		void fp_DetachActor();
		CActor *fp_GetActorRelaxed() const;

		static bool fsp_ScheduleActorDestroy(CActorHolder *_pActorHolder);

	public:
		CActorHolder(CConcurrencyManager *_pConcurrencyManager, bool _bImmediateDelete, EPriority _Priority, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);
		virtual ~CActorHolder();

		void f_Yield();
		void f_SetFixedCore(mint _iFixedCore);
		void f_QueueProcessDestroy(FActorQueueDispatch &&_Functor);
		void f_QueueProcess(FActorQueueDispatch &&_Functor);
		bool f_ImmediateDelete() const;
		bool f_IsAlwaysAlive() const;
		bool f_IsHolderDestroyed() const;
		virtual bool f_IsProcessedOnActorHolder(CActorHolder const *_pActorHolder) const;
#if DMibEnableSafeCheck > 0
		virtual bool f_CurrentlyProcessing() const;
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

		template
			<
				typename tf_CActor
				, typename tf_CFunction
				, typename tf_CFunctor
				, typename tf_CResultActor
				, typename tf_CResultFunctor
			>
		DMibSuppressUndefinedSanitizer bool f_Call
			(
				tf_CFunctor &&_fToCall
				, TCActor<tf_CResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		;

		NStorage::CIntrusiveRefCountWithWeak m_RefCount;

	private:
		void fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory);

		void fp_DestroyActorHolder
			(
				NFunction::TCFunctionNoAllocMovable<void ()> &&_fOnDestroyed
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

			COrdering_Strong operator <=> (COnTerminate const &_Other) const
			{
				return this <=> &_Other;
			}
		};

		// Alignment zone 1 = 8+16+8+8 = 40 => 64 bytes
		NStorage::TCSharedPointer<ICDistributedActorData> mp_pDistributedActorData;
		NContainer::TCSet<COnTerminate> mp_OnTerminate;

		// Alignment zone 2 = 8+8 = 16 => 64 bytes
		align_cacheline CConcurrentRunQueue mp_ConcurrentRunQueue;
		NAtomic::TCAtomic<mint> mp_Working;

	private:
#if DMibConfig_Concurrency_DebugBlockDestroy
		DMibListLinkDS_Link(CActorHolder, m_ActorLink);
	public:
		NStr::CStr m_ActorTypeName;
#endif
		DMibRefCountDebuggingOnly(NStorage::CRefCountDebugReference m_DebugSelfRef);

	protected:
		// Alignment zone 3 = 8+8+8+8+4+1+1 = 38 => 64 bytes
		align_cacheline CConcurrentRunQueue::CLocalQueueData mp_ConcurrentRunQueueLocal;
		CConcurrencyManager *mp_pConcurrencyManager;

		NAtomic::TCAtomic<CActor *> mp_pActorUnsafe = nullptr;
		mutable NAtomic::TCAtomic<smint> mp_bDestroyed;

		uint32 mp_iFixedCore = 0;

		// Note: Changing these is not thread safe
		uint8 mp_Priority:1 = 0;
		uint8 mp_bImmediateDelete:1 = false;
		uint8 mp_bIsAlwaysAlive:1 = false;
		uint8 mp_bHasOverriddenDestroy:1 = false;

		bool mp_bYield = false;

		// Alignment zone 4: Actor storage, other actor holder data
	};

#if DMibPMemoryCacheLineSize == 64 && DMibConfig_RefCountDebugging != 1
	static_assert(sizeof(CActorHolder) == 64*3);
#endif

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
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
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
				, NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> &&_Dispatcher
			)
		;

	protected:
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;

		NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> mp_Dispatcher;
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

		bool f_IsProcessedOnActorHolder(CActorHolder const *_pActorHolder) const override;
#if DMibEnableSafeCheck > 0
		bool f_CurrentlyProcessing() const override;
#endif

	protected:
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;

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
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;

		NThread::CLowLevelLock mp_ThreadLock;
		NStorage::TCUniquePointer<NThread::CThreadObject> mp_pThread;
		NStr::CStr mp_ThreadName;
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
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
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
		void fp_QueueProcess(FActorQueueDispatch &&_Functor, CConcurrencyThreadLocal &_ThreadLocal) override;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
