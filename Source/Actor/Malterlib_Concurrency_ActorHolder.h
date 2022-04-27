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
		struct align_cacheline CQueueEntry
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

		template <typename t_CImplementation>
		friend struct TCDistributedActorInstance;

		template <typename tf_CToDelete>
		friend void fg_DeleteWeakObject(CInternalActorAllocator &, tf_CToDelete *_pObject)
			requires (NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value)
		;

		struct CDestroyHandler;

	protected:
		bool fp_AddToQueue(FActorQueueDispatch &&_Functor);
		virtual void fp_StartQueueProcessing();

		template <typename tf_CActor>
		TCActor<tf_CActor> fp_GetAsActor();

		virtual void fp_DestroyThreaded();
		virtual void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) = 0;
		virtual void fp_QueueProcess(FActorQueueDispatch &&_Functor) = 0;
		void fp_RunProcess();
		virtual TCActorInternal<CActor> *fp_GetRealActor(NConcurrency::CActorHolder *_pActorInternal) const = 0;
		static auto fsp_DestroyHandler(TCActorHolderSharedPointer<CActorHolder> &&_pActorHolder, TCPromise<void> &_Promise);

		void fp_DeleteActor();
		void fp_DetachActor();
		CActor *fp_GetActorRelaxed() const;

		static bool fsp_ScheduleActorHolderDestroy(CActorHolder *_pActorHolder, bool &o_bImmediateDelete);
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
		bool f_Call
			(
				tf_CFunctor &&_fToCall
				, TCActor<tf_CResultActor> &&_ResultActor
				, tf_CResultFunctor &&_fResultFunctor
			)
		;

		NStorage::CIntrusiveRefCountWithWeak m_RefCount;

	private:
		void fp_ConstructActor(NFunction::TCFunctionNoAllocMovable<void ()> &&_fConstruct, void *_pActorMemory);

		void fp_DestroyActorHolder(NFunction::TCFunctionNoAllocMovable<void ()> &&_fOnDestroyed, TCActorHolderSharedPointer<CActorHolder> &&_pSelfReference);
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
		// Alignment zone 3 = 8+8+8+8+8 = 40 => 64 bytes
		align_cacheline CConcurrentRunQueue::CLocalQueueData mp_ConcurrentRunQueueLocal;
		CConcurrencyManager *mp_pConcurrencyManager;
		mint mp_iFixedCore:sizeof(mint)*8 - 5 = 0;
		mint mp_Priority:1 = 0;
		mint mp_bImmediateDelete:1 = false;
		mint mp_bYield:1 = false;
		mint mp_bIsAlwaysAlive:1 = false;
		mint mp_bHasOverriddenDestroy:1 = false;

		NAtomic::TCAtomic<CActor *> mp_pActorUnsafe = nullptr;
		mutable NAtomic::TCAtomic<smint> mp_bDestroyed;

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
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;
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
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;

		NFunction::TCFunctionMovable<void (FActorQueueDispatch &&_Dispatch)> m_Dispatcher;
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
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;

		TCActorHolderSharedPointer<CActorHolder> mp_pDelegateTo;
		COnTerminate *mp_pOnTerminateEntry = nullptr;
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

	protected:
		void fp_StartQueueProcessing() override;
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;

		NStorage::TCUniquePointer<NThread::CThreadObject> m_pThread;
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
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;
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
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
