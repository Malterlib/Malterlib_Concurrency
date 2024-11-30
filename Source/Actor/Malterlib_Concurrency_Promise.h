// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Coroutine.h"
#include "Malterlib_Concurrency_ExceptionTransform.h"
#include "Malterlib_Concurrency_WeakActor.h"
#include "Malterlib_Concurrency_ActorHolder.h"

#include <Mib/Core/CoroutineFlags>

#define DMibConcurrencyNonAtomicOnResultSet

namespace NMib::NConcurrency
{
	struct CActor;

	template <typename t_CActor>
	class TCActor;

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CReturnValue>
	struct TCPromise;

	template <typename t_CReturnValue>
	struct TCFuture;

	struct CRunLoop;
}

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		struct CPromiseDataBase;
	}
	struct CPromiseConstructConsume
	{
	};

	struct CPromiseConstructEmpty
	{
	};

	struct CPromiseConstructNoConsume
	{
	};

	struct CPromiseConstructDiscardResult
	{
	};

	struct CPromiseConstructDirectFuture
	{
	};

	enum EFutureResultFlag : uint8
	{
		EFutureResultFlag_None = 0
		, EFutureResultFlag_DataSet = DMibBit(0)
		, EFutureResultFlag_ResultFunctorSet = DMibBit(1)
		, EFutureResultFlag_DiscardResult = DMibBit(2)
	};

	struct CCoroutineOnResumeScope;

	template <typename t_CThis, ECoroutineFlag t_Flags = ECoroutineFlag_None>
	struct TCFutureForwardThis
	{
		TCFutureForwardThis(t_CThis _This)
			: mp_This(fg_Forward<t_CThis>(_This))
		{
		}

		inline_always bool await_ready() const noexcept
		{
			return true;
		}

		inline_always void await_suspend(TCCoroutineHandle<>) const noexcept
		{
		}

		inline_always t_CThis await_resume() const noexcept
		{
			return fg_Forward<t_CThis>(mp_This);
		}

	private:
		t_CThis mp_This;
	};

	template <typename tf_CThis>
	mark_artificial inline_always TCFutureForwardThis<tf_CThis &&> fg_MoveThis(tf_CThis &_This)
	{
		return TCFutureForwardThis<tf_CThis &&>(fg_Move(_This));
	}

	template <ECoroutineFlag tf_Flags, typename tf_CThis>
	mark_artificial inline_always TCFutureForwardThis<tf_CThis &&, tf_Flags> fg_MoveThis(tf_CThis &_This)
	{
		return TCFutureForwardThis<tf_CThis &&, tf_Flags>(fg_Move(_This));
	}

	template <typename tf_CThis>
	mark_artificial inline_always TCFutureForwardThis<tf_CThis const &> fg_CopyThis(tf_CThis const &_This)
	{
		return TCFutureForwardThis<tf_CThis const &>(_This);
	}

	template <ECoroutineFlag tf_Flags, typename tf_CThis>
	mark_artificial inline_always TCFutureForwardThis<tf_CThis const &, tf_Flags> fg_CopyThis(tf_CThis const &_This)
	{
		return TCFutureForwardThis<tf_CThis const &, tf_Flags>(_This);
	}

#ifdef DMibNeedDebugException
	bool fg_IsDebugException(NException::CExceptionPointer const &_pException);
#endif

	struct CAccessCoroutine
	{
	};

	extern CAccessCoroutine g_AccessCoroutine;

	struct CFutureCoroutineContext;

	struct CAsyncDestroyHelperNoUnWrap
	{
	};

	struct CAsyncDestroyHelper
	{
		CAsyncDestroyHelperNoUnWrap f_Wrap() const
		{
			return CAsyncDestroyHelperNoUnWrap();
		}
	};

	class CActorHolder;

	extern CAsyncDestroyHelper g_AsyncDestroy;

	struct CCaptureExceptionsHelperWithTransformer;
	struct CCaptureExceptionsHelper
	{
		template <typename ...tfp_CExceptions>
		CCaptureExceptionsHelperWithTransformer f_Specific() const;

		operator CCaptureExceptionsHelperWithTransformer const &();
	};

	struct CCaptureExceptionsHelperWithTransformer
	{
		FExceptionTransformer m_fTransformer;
		static CCaptureExceptionsHelperWithTransformer ms_EmptyTransformer;
	};

	extern CCaptureExceptionsHelper g_CaptureExceptions;

	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCFutureAwaiter;

	using FRestoreScopesState = NContainer::TCVector<NFunction::TCFunctionMovable<void () noexcept>>;

	struct CFutureCoroutineContext
	{
		struct [[nodiscard("You need to capture the scope")]] CCaptureExceptionScope : public CCoroutineThreadLocalHandler
		{
			CCaptureExceptionScope(CFutureCoroutineContext *_pThis);
			CCaptureExceptionScope(CCaptureExceptionScope &&_Other);
			~CCaptureExceptionScope();
			void f_Suspend() noexcept override;
			void f_ResumeNoExcept() noexcept override;

		private:
			CFutureCoroutineContext *mp_pThis;
			int mp_OriginalExceptions;
		};

		struct [[nodiscard("You need to capture the scope")]] CCaptureExceptionWithTransformerScope : public CCoroutineThreadLocalHandler
		{
			CCaptureExceptionWithTransformerScope
				(
					CFutureCoroutineContext *_pThis
					, FExceptionTransformer &&_fTransformer
				)
			;
			CCaptureExceptionWithTransformerScope(CCaptureExceptionWithTransformerScope &&_Other);
			~CCaptureExceptionWithTransformerScope();
			void f_Suspend() noexcept override;
			void f_ResumeNoExcept() noexcept override;

		private:
			CFutureCoroutineContext *mp_pThis;
			FExceptionTransformer mp_fTransformer;
			int mp_OriginalExceptions;
		};

		struct CCaptureExceptionScopeAwaiter : public CSuspendNever
		{
			CCaptureExceptionScope await_resume() const noexcept;

			CFutureCoroutineContext *m_pThis;
		};

		struct CCaptureExceptionScopeWithTransformerAwaiter : public CSuspendNever
		{
			CCaptureExceptionWithTransformerScope await_resume() noexcept;

			CFutureCoroutineContext *m_pThis;
			FExceptionTransformer m_fTransformer;
		};

		struct COnResumeScopeAwaiter : public CSuspendNever
		{
			COnResumeScopeAwaiter(COnResumeScopeAwaiter &&) = delete;
			COnResumeScopeAwaiter(COnResumeScopeAwaiter const &) = delete;
			~COnResumeScopeAwaiter()
#if DMibEnableSafeCheck > 0
				noexcept(false)
#endif
			;

			constexpr bool await_ready() const noexcept
			{
				return false;
			}

			template <typename tf_CCoroutineContext>
			bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle);
			CCoroutineOnResumeScope await_resume() noexcept;

		private:
			friend struct CFutureCoroutineContext;
			friend COnResumeScopeAwaiter fg_OnResume(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume);

			COnResumeScopeAwaiter(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume);

			NFunction::TCFunctionMovable<NException::CExceptionPointer ()> mp_fOnResume;
			CFutureCoroutineContext *mp_pThis = nullptr;
#if DMibEnableSafeCheck > 0
			bool mp_bAwaited = false;
#endif
		};

		using FDeliverExceptionResult = NFunction::TCFunctionSmallMovable<void (NException::CExceptionPointer &&_pException)>;
		using FKeepaliveSetException = FDeliverExceptionResult (CFutureCoroutineContext &_Context, TCActor<CActor> &&_Actor);

		CFutureCoroutineContext(ECoroutineFlag _Flags) noexcept;
		~CFutureCoroutineContext() noexcept;

		struct CInitialSuspend;
		struct CFinalAwaiter;

		CInitialSuspend initial_suspend() noexcept;
		CFinalAwaiter final_suspend() noexcept;

		template <typename t_CThis, ECoroutineFlag tf_Flags>
		decltype(auto) await_transform(TCFutureForwardThis<t_CThis, tf_Flags> &&_MoveThis)
		{
#ifndef DMibClangAnalyzerWorkaround
			DMibFastCheck(m_Flags & ECoroutineFlag_UnsafeThisPointer); // You don't need to move this

#if DMibEnableSafeCheck > 0
			if constexpr ((tf_Flags & (ECoroutineFlag_CaptureExceptions | ECoroutineFlag_CaptureMalterlibExceptions | ECoroutineFlag_AllowReferences)) != ECoroutineFlag_None)
				m_Flags |= tf_Flags;
			if (!(m_Flags & ECoroutineFlag_UnsafeReferenceParameters))
				m_Flags |= ECoroutineFlag_AllowReferences;
#else
			if constexpr ((tf_Flags & (ECoroutineFlag_CaptureExceptions | ECoroutineFlag_CaptureMalterlibExceptions)) != ECoroutineFlag_None)
				m_Flags |= tf_Flags;
#endif
#endif
			return fg_Move(_MoveThis);
		}

		CSuspendNever await_transform(ECoroutineFlag _Flags)
		{
#ifndef DMibClangAnalyzerWorkaround
			m_Flags |= _Flags;
#endif
			return {};
		}

		CCaptureExceptionScopeAwaiter await_transform(CCaptureExceptionsHelper);
		CCaptureExceptionScopeWithTransformerAwaiter await_transform(CCaptureExceptionsHelperWithTransformer &&_Transformer);

		TCFutureAwaiter<void, true, CVoidTag> await_transform(CAsyncDestroyHelper);
		TCFutureAwaiter<NContainer::TCVector<TCAsyncResult<void>>, true, CVoidTag> await_transform(CAsyncDestroyHelperNoUnWrap);
		COnResumeScopeAwaiter &&await_transform(COnResumeScopeAwaiter &&_Awaiter);

		void unhandled_exception();

		static void *fs_New(mint _Size, mint _Alignment, mint _PromiseDataSize, mint _PromiseDataAlignment);
		static void fs_Delete(void *_pMemory, mint _Size, mint _Alignment);

		void f_HandleAwaitedException(CConcurrencyThreadLocal &_ThreadLocal, FKeepaliveSetException *_fSetException, NException::CExceptionPointer &&_pException);
		FDeliverExceptionResult f_PrepareExceptionResult(FKeepaliveSetException *_fSetException, TCActor<CActor> &&_Actor);

		template <typename t_CAwaitable>
		decltype(auto) await_transform(t_CAwaitable &&_Awaitable)
		{
			return fg_Forward<t_CAwaitable>(_Awaitable);
		}

		void f_Suspend(CSystemThreadLocal &_ThreadLocal, bool _bAddToActor)
#if DMibEnableSafeCheck <= 0
			noexcept
#endif
		;
		FRestoreScopesState f_Resume(CSystemThreadLocal &_ThreadLocal, FKeepaliveSetException *_fSetException, bool &o_bAborted);

		void f_InitialSuspend(CSystemThreadLocal &_ThreadLocal);
		void f_ResumeInitialSuspend(CSystemThreadLocal &_ThreadLocal, FRestoreScopesState &o_RestoreScopes);
		void f_Abort();

#if DMibEnableSafeCheck > 0
		void f_SetOwner(CActorHolder *_pCoroutineOwner);
#endif

		void f_SetExceptionResult(NException::CExceptionPointer &&_pException);
		bool f_IsException();

		// Needed always: 5 pointers
		NPrivate::CPromiseDataBase *m_pPromiseData = nullptr;
		FRestoreScopesState m_RestoreScopes;
		CCoroutineHandler m_CoroutineHandlers;
		NStorage::TCUniquePointer<CPromiseKeepAlive> m_pKeepAlive;
		ECoroutineFlag m_Flags = ECoroutineFlag_None;
		bool m_bAwaitingInitialResume:1 = false;
		bool m_bRuntimeStateConstructed:1 = false;
		bool m_bLinkConstructed:1 = false;
		bool m_bPreviousCoroutineHandlerConstructed:1 = false;

		struct CRuntimeState
		{
			CRuntimeState(CCoroutineHandler *_pPreviousCoroutineHandler)
				: m_pPreviousCoroutineHandler(_pPreviousCoroutineHandler)
			{
			}

			union
			{
				NMib::NIntrusive::CDLinkAggregateListNoPrevPtr m_Link;
				CCoroutineHandler *m_pPreviousCoroutineHandler;
			};
			NContainer::TCVector<TCFuture<void>> m_AsyncDestructors;
		};

		union
		{
			// Needed after initial resume: 3 pointers
			CRuntimeState m_State;
			// Needed before initial resume: 3 pointers
#if DMibEnableSafeCheck > 0
			void *m_RunQueueImplStorage[4];
#else
			void *m_RunQueueImplStorage[3];
#endif
		};

		struct CDLinkTranslatorm_Link
		{
			template <typename t_CClass>
			struct TCOffset
			{
				enum
				{
					mc_Offset = DMibPOffsetOf(t_CClass, m_State.m_Link)
				};
			};
		};

#if DMibEnableSafeCheck > 0
		bool m_bIsCalledSafely = false;
		NException::CExceptionPointer m_pSuspendDebugException;
		static bool ms_bDebugCoroutineSafeCheck;
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		NException::CCallstack m_CreateCallstack;
#endif
	protected:
		void fp_GetReturnObjectShared
			(
				NPrivate::CPromiseDataBase *_pPromiseData
				, CPromiseThreadLocal &_ThreadLocal
#if DMibEnableSafeCheck > 0
				, bool _bGenerator
#endif
			)
		;

		bool fp_DestroyShared(NPrivate::CPromiseDataBase *_pPromiseData);

#if DMibEnableSafeCheck > 0
		inline_never void fp_UpdateSafeCall(bool _bSafeCall);

		inline_never void fp_CheckUnsafeThisPointer();
		inline_never void fp_CheckCaptureExceptions() const;
#endif
	};
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_FFunctor>
	struct TCAllAsyncResultsAreVoid;

	template <typename t_CPromise>
	struct TCPromiseReceiveAnyFunctor;

	struct CPromiseReceiveAnyUnwrapFunctor;

	template <typename t_CReturnValue>
	struct TCPromiseData;

	template <typename t_CReturnType>
	struct TCFutureCoroutineContextShared;

	template <typename t_CReturnType>
	struct TCFutureCoroutineKeepAliveImplicit
	{
		TCFutureCoroutineKeepAliveImplicit(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAliveImplicit(TCFutureCoroutineKeepAliveImplicit &&_Other);
		TCFutureCoroutineKeepAliveImplicit(TCFutureCoroutineKeepAliveImplicit const &) = delete;
		~TCFutureCoroutineKeepAliveImplicit();

		TCFutureCoroutineKeepAliveImplicit f_Copy() const;

		bool f_HasValidCoroutine() const;

		template <typename tf_CCoroutineContext>
		TCCoroutineHandle<tf_CCoroutineContext> f_Coroutine()
		{
			DMibFastCheck(mp_pPromiseData);
			return TCCoroutineHandle<tf_CCoroutineContext>::from_address(mp_pPromiseData->m_Coroutine.address());
		}

		NPrivate::TCPromiseData<t_CReturnType> &f_PromiseData()
		{
			return *mp_pPromiseData;
		}

	private:
		friend struct TCFutureCoroutineContextShared<t_CReturnType>;

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> mp_pPromiseData;
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineKeepAlive : public TCFutureCoroutineKeepAliveImplicit<t_CReturnType>
	{
		TCFutureCoroutineKeepAlive(TCActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAlive(TCWeakActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAlive(TCFutureCoroutineKeepAlive &&);
		TCFutureCoroutineKeepAlive(TCFutureCoroutineKeepAlive const &) = delete;
		~TCFutureCoroutineKeepAlive();

		TCActor<CActor> f_MoveActor();

		TCFutureCoroutineKeepAliveImplicit<t_CReturnType> &&f_MoveImplicit();

	private:
		friend struct TCFutureCoroutineContextShared<t_CReturnType>;

		NStorage::TCBitStorePointer<TCActorInternal<CActor>> mp_pActor;
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContextShared : public CFutureCoroutineContext
	{
		using CReturnType = t_CReturnType;

		struct CConcurrentRunQueueEntryImpl final : public CConcurrentRunQueueEntry
		{
#if DMibEnableSafeCheck > 0
			CActorHolder *m_pResumeActor = nullptr;
#endif
			void f_Call(CConcurrencyThreadLocal &_ThreadLocal) override;
			void f_Cleanup() override;
		};

		TCFutureCoroutineContextShared(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: CFutureCoroutineContext(_Flags)
		{
		}

		TCFutureCoroutineContextShared(TCFutureCoroutineContextShared &&) = delete;
		~TCFutureCoroutineContextShared() noexcept;

		TCFutureCoroutineKeepAlive<t_CReturnType> f_KeepAlive(TCActor<> &&_Actor);
		TCFutureCoroutineKeepAliveImplicit<t_CReturnType> f_KeepAliveImplicit();

		mark_no_coroutine_debug TCFuture<t_CReturnType> get_return_object();

		CConcurrentRunQueueEntryImpl *f_ConstructRunQueueEntry();

		static FDeliverExceptionResult fs_KeepaliveSetExceptionFunctor(CFutureCoroutineContext &_Context, TCActor<CActor> &&_Actor);

		struct CAccessCoroutineAwaiter
		{
			constexpr bool await_ready() const noexcept { return true; }
			constexpr void await_suspend(auto &&) const noexcept {}
			constexpr TCFutureCoroutineContextShared &await_resume() const noexcept
			{
				return m_This;
			}

			TCFutureCoroutineContextShared &m_This;
		};

		using CFutureCoroutineContext::await_transform;

		CAccessCoroutineAwaiter await_transform(CAccessCoroutine _Coroutine)
		{
			return CAccessCoroutineAwaiter{.m_This = *this};
		}

		inline_always TCPromiseData<t_CReturnType> *f_PromiseData()
		{
			return static_cast<TCPromiseData<t_CReturnType> *>(m_pPromiseData);
		}

	protected:
		void fp_CleanupDirectResume();

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> fp_GetReturnObject();
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> fp_GetReturnObjectGenerator(CPromiseThreadLocal &_ThreadLocal);
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContextAllocation : public TCFutureCoroutineContextShared<t_CReturnType>
	{
		TCFutureCoroutineContextAllocation(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: TCFutureCoroutineContextShared<t_CReturnType>(_Flags)
		{
		}

		inline_always void *operator new(size_t _Size, std::align_val_t _Alignment)
		{
			return CFutureCoroutineContext::fs_New(_Size, (size_t)_Alignment, sizeof(TCPromiseData<t_CReturnType>), alignof(TCPromiseData<t_CReturnType>));
		}

		inline_always void operator delete(void *_pMemory, size_t _Size, std::align_val_t _Alignment)
		{
			return CFutureCoroutineContext::fs_Delete(_pMemory, _Size, (size_t)_Alignment);
		}

		inline_always void *operator new(size_t _Size)
		{
			return CFutureCoroutineContext::fs_New(_Size, mint(1) << NMib::fg_GetLowestBitSetNoZero(_Size), sizeof(TCPromiseData<t_CReturnType>), alignof(TCPromiseData<t_CReturnType>));
		}

		inline_always void operator delete(void *_pMemory, size_t _Size)
		{
			return CFutureCoroutineContext::fs_Delete(_pMemory, _Size, mint(1) << NMib::fg_GetLowestBitSetNoZero(_Size));
		}
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContext : public TCFutureCoroutineContextAllocation<t_CReturnType>
	{
		TCFutureCoroutineContext(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: TCFutureCoroutineContextAllocation<t_CReturnType>(_Flags)
		{
		}

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
		void return_value(t_CReturnType &&_Value);
	};

	template <>
	struct TCFutureCoroutineContext<void> : public TCFutureCoroutineContextAllocation<void>
	{
		TCFutureCoroutineContext(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: TCFutureCoroutineContextAllocation<void>(_Flags)
		{
		}

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
		void return_value(CVoidTag &&_Value);
		void return_value(CVoidTag const &_Value);
	};

	template <typename t_CReturnType, ECoroutineFlag t_Flags = ECoroutineFlag_None>
	struct TCFutureCoroutineContextWithFlags : public TCFutureCoroutineContext<t_CReturnType>
	{
		TCFutureCoroutineContextWithFlags() noexcept
			: TCFutureCoroutineContext<t_CReturnType>(t_Flags)
		{
		}

		static constexpr ECoroutineFlag mc_InitialFlags = t_Flags;
	};

	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception, NStr::CStr const &_CallStack = {});

	enum EConsumeFutureOnResultSetFlag : uint32
	{
		EConsumeFutureOnResultSetFlag_None = 0
		, EConsumeFutureOnResultSetFlag_ResetSafeCall = DMibBit(0)
	};

	template <typename t_CReturnValue>
	struct TCPromiseDataInit
	{
		TCFutureOnResult<t_CReturnValue> *m_pOnResult;
		uint8 m_Flags = 0;
		bool m_bOnResultSetAtInit = false;
	};

	template <typename tf_CReturnValue>
	TCPromiseDataInit<tf_CReturnValue> fg_ConsumeFutureOnResultSet
		(
			CPromiseThreadLocal &_ThreadLocal
#if DMibEnableSafeCheck > 0
			, void const *_pConsumedBy
			, EConsumeFutureOnResultSetFlag _Flags
#endif
		)
	;

	enum EConsumePromiseDebugFlag : uint32
	{
		EConsumePromiseDebugFlag_None = 0
		, EConsumePromiseDebugFlag_ResetSafeCall = DMibBit(0)
	};

	enum EConsumePromiseFlag : uint32
	{
		EConsumePromiseFlag_None = 0
		, EConsumePromiseFlag_UsePromise = DMibBit(0)
		, EConsumePromiseFlag_HaveAllocation = DMibBit(2)
		, EConsumePromiseFlag_FromCoroutine = DMibBit(3)
	};

	template <typename tf_CType, EConsumePromiseFlag tf_Flags>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<tf_CType>> fg_ConsumePromise
		(
			CPromiseThreadLocal &_ThreadLocal
			, void *_pAllocation
			, int32 _RefCount
#if DMibEnableSafeCheck > 0
			, void const *_pConsumedBy
			, EConsumePromiseDebugFlag _DebugFlags
#endif
		)
	;

	template <typename t_CType>
	struct TCAlignOfWithVoid
	{
		static constexpr mint mc_Value = alignof(t_CType);
	};

	template <>
	struct TCAlignOfWithVoid<void>
	{
		static constexpr mint mc_Value = 0;
	};

	struct CPromiseDataBaseRefCount : public NStorage::TCIntrusiveRefCount<NStorage::ESharedPointerOption_None, int32>
	{
		constexpr static int32 mc_FutureBits = 2;
		constexpr static int32 mc_FutureOffset = sizeof(int32) * 8 - 3;
		constexpr static int32 mc_FutureBit = DMibBitTyped(mc_FutureOffset, int32);
		constexpr static int32 mc_FutureMask = DMibBitRangeTyped(mc_FutureOffset, mc_FutureOffset + mc_FutureBits - 1, int32);
		constexpr static int32 mc_PromiseMask = DMibBitRangeTyped(0, mc_FutureOffset - 1, int32);

		using NStorage::TCIntrusiveRefCount<NStorage::ESharedPointerOption_None, int32>::TCIntrusiveRefCount;

		static int32 fs_GetFutureCount(int32 _Value)
		{
			return (_Value & mc_FutureMask) >> mc_FutureOffset;
		}

		int32 f_GetFutureCount() const
		{
			return fs_GetFutureCount(m_RefCount.f_Load(NAtomic::EMemoryOrder_Relaxed));
		}

		int32 f_GetPromiseCount() const
		{
			int32 Value = (m_RefCount.f_Load(NAtomic::EMemoryOrder_Relaxed) & mc_PromiseMask) << 3;
			Value = Value >> 3; // Sign extend
			return Value;
		}

		bool f_PromiseToFuture()
		{
			auto OldValue = m_RefCount.f_FetchAdd(mc_FutureBit - 1, NAtomic::EMemoryOrder_Relaxed);
			DMibSafeCheck(fs_GetFutureCount(OldValue) <= 2, "Maximum three futures");

			return (OldValue & mc_PromiseMask) == 0;
		}

		void f_FutureToPromise()
		{
			m_RefCount.f_FetchSub(mc_FutureBit - 1, NAtomic::EMemoryOrder_Relaxed);
		}

		void f_IncreaseFuture()
		{
			[[maybe_unused]] int32 Return = m_RefCount.f_FetchAdd(mc_FutureBit, NAtomic::EMemoryOrder_Relaxed);
			DMibFastCheck(Return >= 0);
			DMibSafeCheck(fs_GetFutureCount(Return) <= 2, "Maximum three futures");
		}

		bool f_DecreaseFuture()
		{
			int32 Return = m_RefCount.f_FetchSub(mc_FutureBit, NAtomic::EMemoryOrder_Release);
			DMibFastCheck(Return >= 0);
			if ((Return - mc_FutureBit) < 0)
			{
	#ifdef DMibSanitizerEnabled_Thread
				m_RefCount.f_Load(NAtomic::EMemoryOrder_Acquire);
	#else
				NAtomic::fg_MemoryFence(NAtomic::EMemoryOrder_Acquire);
	#endif
				return true;
			}

			return false;
		}

		bool f_IsLastPromiseReference()
		{
			// A future cannot be converted into a promise, so this should be safe
			return (m_RefCount.f_Load(NAtomic::EMemoryOrder_Acquire) & mc_PromiseMask) == 0;
		}

		bool f_PromiseDecrease()
		{
			auto OldValue = m_RefCount.f_FetchSub(1, NAtomic::EMemoryOrder_Release);
			if (OldValue == 0)
			{
	#ifdef DMibSanitizerEnabled_Thread
				m_RefCount.f_Load(NAtomic::EMemoryOrder_Acquire);
	#else
				NAtomic::fg_MemoryFence(NAtomic::EMemoryOrder_Acquire);
	#endif
				return true;
			}

			return false;
		}

#if DMibConfig_RefCountDebugging
		int32 f_Increase
			(
				NStorage::CRefCountDebugReference &o_Reference
#if DMibEnableSafeCheck > 0
				, bool _bAllowRevive = false
#endif
			) const
		{
			auto Return = NStorage::TCIntrusiveRefCount<NStorage::ESharedPointerOption_None, int32>::f_Increase
				(
					o_Reference
#if DMibEnableSafeCheck > 0
					, _bAllowRevive
#endif
				)
			;

			DMibSafeCheck((Return & mc_PromiseMask) == mc_PromiseMask || (Return & mc_PromiseMask) < (mc_PromiseMask - 1), "Promise reference overflow");
			return Return;
		}
#else
		int32 f_Increase
			(
#if DMibEnableSafeCheck > 0
				bool _bAllowRevive = false
#endif
			) const
		{
			int32 Return = m_RefCount.f_FetchAdd(1, NAtomic::EMemoryOrder_Relaxed);
			DMibFastCheck(Return >= 0 || _bAllowRevive && Return == -1);
			DMibSafeCheck((Return & mc_PromiseMask) == mc_PromiseMask || (Return & mc_PromiseMask) < (mc_PromiseMask - 1), "Promise reference overflow");
			return Return;
		}
#endif
	};

	struct CPromiseDataBase
	{
		CPromiseDataBase
			(
				int32 _RefCount
				, uint8 _OnResultSet
				, bool _bOnResultSetAtInit
				, uint8 _OverAlignment
#if DMibConfig_Concurrency_DebugFutures
				, NStr::CStr const &_Name
#endif
			)
		;

#if DMibConfig_Concurrency_DebugFutures
		~CPromiseDataBase();
#endif
		inline_always NAtomic::EMemoryOrder f_MemoryOrder(NAtomic::EMemoryOrder _Default) const;

		inline_always bool f_CanUseRelaxedAtomicsForRead() const
		{
			return !m_BeforeSuspend.m_bNeedAtomicRead;
		}

		CAsyncResult &f_GetAsyncResult();
		void f_OnResultPending();
		void f_OnResultPendingAppendable();

		TCCoroutineHandle<CFutureCoroutineContext> m_Coroutine;

		CPromiseDataBaseRefCount m_RefCount;
		NAtomic::TCAtomic<uint8> m_OnResultSet;

		struct CCanOnlyBeSetBeforeSuspend
		{
			uint8 m_bNeedAtomicRead = true;
			uint8 m_bOnResultSetAtInit:1 = false;
			uint8 m_bIsGenerator:1 = false;
			uint8 m_bIsCoroutine:1 = false;
			uint8 m_bAllocatedInCoroutineContext:1 = false;
			uint8 m_bPromiseConsumed:1 = false;
			uint8 m_OverAlignment: 3 = 0;

			void f_NeedAtomicRead();
		};

		CCanOnlyBeSetBeforeSuspend m_BeforeSuspend;

		struct CCanBeSetAfterSuspend
		{
			uint8 m_bPendingResult:1 = false;
			uint8 m_bCoroutineValid:1 = false;
		};

		CCanBeSetAfterSuspend m_AfterSuspend;

#if DMibConfig_Concurrency_DebugFutures
		DMibListLinkDS_Link(CPromiseDataBase, m_Link);
		DMibListLinkDS_Link(CPromiseDataBase, m_LinkCoro);
		NStr::CStr m_FutureTypeName;
#endif
#if DMibEnableSafeCheck > 0
		CActorHolder *m_pCoroutineOwner = nullptr;
		void *m_pHadCoroutine = nullptr;
		bool m_bFutureGotten = false;
#endif
#if DMibConfig_Concurrency_DebugUnobservedException
		NException::CCallstack m_Callstack;
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		NException::CCallstack m_OnResultSetCallstack;
		NException::CCallstack m_ResultSetCallstack;
		NException::CCallstack m_PendingResultSetCallstack;
#endif
	};

	template <typename t_CReturnValue>
	struct TCPromiseData final : public CPromiseDataBase
	{
		using FOnResult = TCFutureOnResult<t_CReturnValue>;

		static_assert(fg_GetHighestBitSet(fg_MaxConstexpr(TCAlignOfWithVoid<t_CReturnValue>::mc_Value, sizeof(void *)) / sizeof(void *)) <= 7); // Max supported over alignment

		TCPromiseData
			(
				CPromiseThreadLocal &_ThreadLocal
				, int32 _RefCount
#if DMibEnableSafeCheck > 0
				, EConsumeFutureOnResultSetFlag _DebugFlags = EConsumeFutureOnResultSetFlag_ResetSafeCall
#endif
			)
		;

		TCPromiseData(TCPromiseDataInit<t_CReturnValue> &&_Init, int32 _RefCount);
		TCPromiseData(FOnResult &&_fOnResult, int32 _RefCount);
		TCPromiseData(CPromiseConstructDiscardResult, int32 _RefCount);
		TCPromiseData(CPromiseConstructDirectFuture);
		~TCPromiseData();

		void f_Reset();

		bool f_IsSet() const;
		bool f_IsDiscarded() const;

		void f_ReportNothingSetAtExit();
		void f_ReportNothingSet();
		void f_OnResult();
		void f_OnResultRelaxed();
		void f_SetResult();
		void f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result);

		void f_SetResultNoReport();
		void f_SetResultNoReport(TCAsyncResult<t_CReturnValue> const &_Result);
		void f_SetResultNoReport(TCAsyncResult<t_CReturnValue> &_Result);
		void f_SetResultNoReport(TCAsyncResult<t_CReturnValue> volatile &_Result);
		void f_SetResultNoReport(TCAsyncResult<t_CReturnValue> const volatile &_Result);
		void f_SetResultNoReport(TCAsyncResult<t_CReturnValue> &&_Result);

		void f_SetResult(TCPromise<t_CReturnValue> const &_Result) = delete;
		void f_SetResult(TCPromise<t_CReturnValue> &_Result) = delete;
		void f_SetResult(TCPromise<t_CReturnValue> &&_Result);


		void f_SetResultNoReport(TCPromise<t_CReturnValue> const &_Result) = delete;
		void f_SetResultNoReport(TCPromise<t_CReturnValue> &_Result) = delete;
		void f_SetResultNoReport(TCPromise<t_CReturnValue> &&_Result);

		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result);

		template <typename tf_CResult>
		void f_SetResultNoReport(tf_CResult &&_Result);

		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> const &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &&_Result);
		void f_SetCurrentException();

		template <typename tf_CResult>
		void f_SetExceptionNoReport(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetExceptionNoReport(TCAsyncResult<tf_CResult> const &_Result);
		template <typename tf_CResult>
		void f_SetExceptionNoReport(TCAsyncResult<tf_CResult> &_Result);
		template <typename tf_CResult>
		void f_SetExceptionNoReport(TCAsyncResult<tf_CResult> &&_Result);
		void f_SetCurrentExceptionNoReportAppendable();

		void f_SetExceptionNoReportAppendable(NException::CExceptionPointer &&_pException);

		TCFutureCoroutineKeepAlive<t_CReturnValue> f_CoroutineKeepAlive(TCActor<> &&_Actor);

		FOnResult m_fOnResult;
		TCAsyncResult<t_CReturnValue> m_Result;
	};
}

template <typename t_CReturnValue>
struct NMib::NStorage::TCHasIntrusiveRefCount<NMib::NConcurrency::NPrivate::TCPromiseData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CReturnValue>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::NPrivate::TCPromiseData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{
};

extern "C" void fg_MalterlibConcurrency_TCFutureFunctionEnter(void *_pThisFunction, void *_pCallSite);

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCPromiseWithExceptionTransfomer;

	template <typename t_CReturnValue, typename t_CFutureLike>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFutureWithExceptionTransformer;

	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform = CVoidTag>
	struct [[nodiscard("You need to co_await the result")]] TCFutureAwaiter;

	template <typename t_CReturnValue = void>
	struct [[nodiscard]] TCPromise;

	#if defined(DCompiler_clang) && DMibEnableSafeCheck > 0
		#if __has_cpp_attribute(clang::instrument_non_coroutine_function_enter)
			#define DMibConcurrencyInstrumentFunctionEnter [[clang::instrument_non_coroutine_function_enter("fg_MalterlibConcurrency_TCFutureFunctionEnter")]]
			#define DMibConcurrencySupportCoroutineFreeFunctionDebug 1
		#else
			#define DMibConcurrencyInstrumentFunctionEnter
		#endif
	#else
		#define DMibConcurrencyInstrumentFunctionEnter
	#endif

	namespace NPrivate
	{
		template <typename t_CReturnType>
		struct TCAsyncGeneratorCoroutineContext;

		template <typename t_CReturnType, ECoroutineFlag t_Flags = ECoroutineFlag_None>
		struct TCAsyncGeneratorCoroutineContextWithFlags;

		template <typename t_CCoroutineContext>
		struct TCIsAsyncGeneratorCoroutineContext
		{
			constexpr static bool mc_Value = false;
		};

		template <typename t_CReturnType>
		struct TCIsAsyncGeneratorCoroutineContext<TCAsyncGeneratorCoroutineContext<t_CReturnType>>
		{
			using CReturnType = t_CReturnType;
			constexpr static bool mc_Value = true;
		};

		template <typename t_CReturnType, ECoroutineFlag t_Flags>
		struct TCIsAsyncGeneratorCoroutineContext<TCAsyncGeneratorCoroutineContextWithFlags<t_CReturnType, t_Flags>>
		{
			using CReturnType = t_CReturnType;
			constexpr static bool mc_Value = true;
		};

		template <typename t_CType>
		struct TCIsActorResultCallHelper : public NTraits::TCCompileTimeConstant<bool, false>
		{
		};

		template <typename t_CActor, typename t_CFunctor>
		struct TCIsActorResultCallHelper<TCActorResultCall<t_CActor, t_CFunctor>> : public NTraits::TCCompileTimeConstant<bool, true>
		{
		};

		template <typename t_CType>
		struct TCIsPromise
		{
			using CType = t_CType;
			static constexpr bool mc_Value = false;
		};

		template <typename t_CType>
		struct TCIsPromise<TCPromise<t_CType>>
		{
			using CType = t_CType;
			static constexpr bool mc_Value = true;
		};

		struct CFutureConstructIgnoreFutureGotten
		{
		};

		struct CFutureConstructSetFutureGotten
		{
		};
	}

	template <typename t_CType>
	struct TCIsActorResultCall : public NPrivate::TCIsActorResultCallHelper<typename NTraits::TCRemoveReference<t_CType>::CType>
	{
	};

	template <typename t_CReturnValue>
	struct TCFuture;

	template <typename t_CType>
	using TCVectorOfFutures = NContainer::TCVector<TCFuture<t_CType>>;

	template <typename t_CKey, typename t_CValue>
	using TCMapOfFutures = NContainer::TCMap<t_CKey, TCFuture<t_CValue>>;

	template <typename t_CKey, typename t_CValue, typename t_CForwardKey>
	struct TCFutureMapBoundKey;

	template <typename t_CReturnValue>
	struct TCPromiseFuturePair;

	/// \brief Used to defer the return of a value to allow async programming
	template <typename t_CReturnValue = void>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] DMibConcurrencyInstrumentFunctionEnter TCFuture
	{
		using CValue = t_CReturnValue;
		using FOnResult = TCFutureOnResult<t_CReturnValue>;

		struct CNoUnwrapAsyncResult
		{
			TCFuture *m_pWrapped;
			auto operator co_await() &&;
		};

		using CInitializationValue = typename TCChooseType<NTraits::TCIsVoid<t_CReturnValue>::mc_Value, CVoidTag, t_CReturnValue>::CType;

		TCFuture();
		~TCFuture();
		TCFuture(NException::CExceptionPointer &&_pException);
		TCFuture(NException::CExceptionBase const &_Exception);
		TCFuture(CInitializationValue &&_ReturnValue) requires(!NTraits::cIsSame<CInitializationValue, NException::CExceptionPointer>);
		TCFuture(CInitializationValue const &_ReturnValue) requires(!NTraits::cIsSame<CInitializationValue, NException::CExceptionPointer>);
		TCFuture(TCAsyncResult<CValue> &&_ReturnValue);
		TCFuture(TCAsyncResult<CValue> const &_ReturnValue);

		TCFuture(TCFuture &&_Other);
		TCFuture &operator = (TCFuture &&_Other);
		TCFuture &operator = (CInitializationValue &&_ReturnValue);
		TCFuture &operator = (CInitializationValue const &_ReturnValue);
		TCFuture &operator = (TCAsyncResult<CValue> &&_ReturnValue);
		TCFuture &operator = (TCAsyncResult<CValue> const &_ReturnValue);

		void f_ForceDelete();
		void f_Clear();

		bool f_OnResultSetOrAvailable(FOnResult &&_fOnResult);
		bool f_ConsumeIfAvailable();
		void f_OnResultSet(FOnResult &&_fOnResult);
		bool f_OnResultSetNoClear(FOnResult &&_fOnResult);
		void f_DiscardResult();
		bool f_ObserveIfAvailable();
		bool f_ObserveIfAvailableRelaxed();
		TCAsyncResult<t_CReturnValue> &&f_MoveResult();

		TCFuture<t_CReturnValue> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true) &&;

		auto f_CallSync() &&;
		auto f_CallSync(fp64 _Timeout) &&;
		auto f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout = -1.0) &&;

		TCFuture &&f_Move();

		bool f_IsValid() const;

		template<typename tf_CFunctor>
		void operator > (tf_CFunctor &&_Functor) &&
			requires (NTraits::TCIsCallableWith<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>::CType, void (TCAsyncResult<t_CReturnValue> &&)>::mc_Value)
		;

		template <typename tf_CResultActor, typename tf_CResultFunctor>
		void operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
			requires (NTraits::TCIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<t_CReturnValue> &&)>::mc_Value) // Incorrect type in result call
		;

		template <typename tf_CReturnValue>
		void operator > (TCPromise<tf_CReturnValue> &&_Promise) &&;

		template <typename tf_CResult>
		void operator > (TCPromiseWithExceptionTransfomer<tf_CResult> &&_PromiseWithError) &&;

		void operator > (TCFutureVector<t_CReturnValue> &_FutureVector) &&;
		void operator > (TCVectorOfFutures<t_CReturnValue> &_ResultVector) &&;
		void operator > (NContainer::TCMapResult<TCFuture<t_CReturnValue> &> &&_MapResult) &&;

		template <typename tf_FOnResult>
		void operator > (TCDirectResultFunctor<tf_FOnResult> &&_fOnResult) &&;

		template <typename tf_CKey, typename tf_CForwardKey>
		inline_always void operator > (TCFutureMapBoundKey<tf_CKey, t_CReturnValue, tf_CForwardKey> &&_BoundKey) &&;

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> &&_Other) &&;

		template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
		auto operator + (TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other) &&;

		template <typename tf_CReturn, typename tf_CFutureLike>
		auto operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other) &&;

		auto operator % (NStr::CStr const &_ErrorString) && -> TCFutureWithExceptionTransformer<t_CReturnValue, TCFuture>;

		CNoUnwrapAsyncResult f_Wrap() &&;
		auto operator co_await() &&;

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &f_Unsafe_PromiseData() const;
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&f_Unsafe_MovePromiseData() &&;

#if DMibEnableSafeCheck > 0
		bool f_Debug_HasData(void const *_pData) const;
		bool f_Debug_HadCoroutine(void const *_pData) const;
#endif

	private:
		friend struct TCPromise<t_CReturnValue>;
		friend struct TCPromiseFuturePair<t_CReturnValue>;

		template <typename t_CReturnType>
		friend struct NPrivate::TCFutureCoroutineContextShared;
		template <typename t_CReturnType>
		friend struct NPrivate::TCAsyncGeneratorCoroutineContext;

		static NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> fsp_CreateData();

		TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData);
		TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData, NPrivate::CFutureConstructIgnoreFutureGotten);
		TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData, NPrivate::CFutureConstructSetFutureGotten);

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> mp_pData;
	};

	template <typename t_CReturnValue, ECoroutineFlag t_Flags>
	struct TCFutureWithFlags : public TCFuture<t_CReturnValue>
	{
		TCFutureWithFlags(TCFuture<t_CReturnValue> &&_Other)
			: TCFuture<t_CReturnValue>(fg_Move(_Other))
		{
		}
	};

	using CUnsafeFuture = TCUnsafeFuture<void>;

	CConcurrencyThreadLocal &fg_ConcurrencyThreadLocal();

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromise
	{
	public:
		using FOnResult = typename NPrivate::TCPromiseData<t_CReturnValue>::FOnResult;

		TCPromise(TCPromise const &_Other);
		TCPromise(TCPromise &&_Other);
		TCPromise &operator =(TCPromise const &_Other);
		TCPromise &operator =(TCPromise &&_Other);
		TCPromise(CConcurrencyThreadLocal &_ThreadLocal = fg_ConcurrencyThreadLocal());
		~TCPromise();

		TCPromise(CPromiseConstructConsume, CConcurrencyThreadLocal &_ThreadLocal = fg_ConcurrencyThreadLocal());
		TCPromise(CPromiseConstructEmpty);
		TCPromise(CPromiseConstructEmpty, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal);
		TCPromise(CPromiseConstructNoConsume, int32 _RefCount = 0, CConcurrencyThreadLocal &_ThreadLocal = fg_ConcurrencyThreadLocal());
		TCPromise(CPromiseConstructDiscardResult);
		TCPromise(CPromiseConstructDiscardResult, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal);
		TCPromise(FOnResult &&_fOnResult);
		TCPromise(FOnResult &&_fOnResult, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal);

		bool f_IsSet() const;
		bool f_IsDiscarded() const;
		bool f_IsValid() const;
		void f_SetResult() const;
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result) const;
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result) const;
		template <typename tf_CException, typename tf_CResult>
		void f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const;
		void f_SetCurrentException() const;
		auto f_ReceiveAny() const & -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise>;
		auto f_ReceiveAnyUnwrap() const & -> NPrivate::CPromiseReceiveAnyUnwrapFunctor;
		auto f_ReceiveAny() && -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise>;
		auto f_ReceiveAnyUnwrap() && -> NPrivate::CPromiseReceiveAnyUnwrapFunctor;
		template <typename tf_FOnEmpty>
		void f_Abandon(tf_FOnEmpty &&_fOnEmpty);
		TCAsyncResult<t_CReturnValue> &&f_MoveResult();

		void f_Clear();
		void f_ClearResultSet();
		TCPromise &&f_Move();
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_Future() const;
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_MoveFuture();

		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_FutureConsumed() const;
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_MoveFutureConsumed();
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_MoveFutureConsumedResultSet();

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &f_Unsafe_PromiseData() const;

		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (CVoidTag const &);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (TCFuture<t_CReturnValue> &&_Future);

		template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<=
			(
				TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_BoundActorCall
			)
		;

		template <typename tf_CResult, TCEnableIfType<!NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CResult>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (tf_CResult &&_Result);
		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (tf_CType &&_Exception);

		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer &&_pException);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer &_pException);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer const &_pException);

		template <typename tf_FResultHandler>
		auto operator / (tf_FResultHandler &&_fResultHandler) const &;
		
		template <typename tf_FResultHandler>
		auto operator / (tf_FResultHandler &&_fResultHandler) &&;

		auto operator % (NStr::CStr const &_ErrorString) const -> TCPromiseWithExceptionTransfomer<t_CReturnValue>;

		mark_no_coroutine_debug TCFuture<t_CReturnValue> fp_AttachFutureIgnoreFutureGotten();

#if DMibConfig_Concurrency_DebugActorCallstacks
		void f_Debug_SetCallstacks(CAsyncCallstacks &&_Callstacks);
#endif

#if DMibEnableSafeCheck > 0
		void const *f_Debug_GetData();
		bool f_Debug_HadCoroutine(void const *_pData) const;
#endif
	private:
		friend struct TCFuture<t_CReturnValue>;
		friend struct TCPromiseFuturePair<t_CReturnValue>;

		template <typename tf_CType, NPrivate::EConsumePromiseFlag tf_Flags>
		friend NStorage::TCSharedPointer<NPrivate::TCPromiseData<tf_CType>> NPrivate::fg_ConsumePromise
			(
				CPromiseThreadLocal &_ThreadLocal
				, void *_pAllocation
				, int32 _RefCount
	#if DMibEnableSafeCheck > 0
				, void const *_pConsumedBy
				, NPrivate::EConsumePromiseDebugFlag _DebugFlags
	#endif
			)
		;

		TCPromise(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &_pData);

	private:
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> mp_pData;
	};

	template <typename t_CReturnValue>
	struct TCPromiseFuturePair
	{
		TCPromise<t_CReturnValue> m_Promise{CPromiseConstructNoConsume{}, NPrivate::CPromiseDataBaseRefCount::mc_FutureBit};
		TCFuture<t_CReturnValue> m_Future{fg_Attach(m_Promise.mp_pData.f_Get()), NPrivate::CFutureConstructSetFutureGotten{}};
	};

	struct CActorWithErrorBase
	{
		CActorWithErrorBase(FExceptionTransformer &&_fTransfomer)
			: m_fTransformer(fg_Move(_fTransfomer))
		{
		}

		FExceptionTransformer f_GetTransformer() &&;
		FExceptionTransformer m_fTransformer;
	};

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromiseWithExceptionTransfomer : public CActorWithErrorBase
	{
		TCPromiseWithExceptionTransfomer(TCPromise<t_CReturnValue> const &_Promise, FExceptionTransformer &&_fTransformer);

		template <typename tf_FResultHandler>
		auto operator / (tf_FResultHandler &&_fResultHandler) &&;

		TCPromise<t_CReturnValue> m_Promise;
	};

	template <typename t_CReturnValue, typename t_CFutureLike>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFutureWithExceptionTransformer : public CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCFutureWithExceptionTransformer *m_pWrapped;
			auto operator co_await() &&;
		};

		TCFutureWithExceptionTransformer(t_CFutureLike &&_Future, FExceptionTransformer &&_fTransfomer);

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> &&_Other) &&;

		template <typename tf_CReturn, typename tf_CFutureLike>
		auto operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other) &&;

		template <typename tf_CRight>
		auto operator > (tf_CRight &&_Right) &&;

		auto operator co_await() &&;
		CNoUnwrapAsyncResult f_Wrap() &&;

		TCFuture<t_CReturnValue> f_Dispatch() &&;

		t_CFutureLike m_Future;
	};

	using FUnitVoidFutureFunction = NFunction::TCFunctionMovable<TCFuture<void> ()>;

	struct [[nodiscard("You should store the on suspend scope")]] CCoroutineOnSuspendScope final : public CCoroutineThreadLocalHandler
	{
		CCoroutineOnSuspendScope(NFunction::TCFunctionMovable<void ()> &&_fOnSuspend);
		~CCoroutineOnSuspendScope();
		void f_Suspend() noexcept override;
		void f_ResumeNoExcept() noexcept override;
		void operator ()();

	private:
		NFunction::TCFunctionMovable<void ()> m_fOnSuspend;
	};

	struct [[nodiscard("You should store the on resume scope")]] CCoroutineOnResumeScope final : public CCoroutineThreadLocalHandler
	{
		CCoroutineOnResumeScope(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume);
		~CCoroutineOnResumeScope();
		void f_Suspend() noexcept override;
		NException::CExceptionPointer f_Resume() noexcept override;

	private:
		NFunction::TCFunctionMovable<NException::CExceptionPointer ()> mp_fOnResume;
	};

	struct CCoroutineOnSuspendHelper
	{
		CCoroutineOnSuspendScope operator / (NFunction::TCFunctionMovable<void ()> &&_fOnSuspend) const;
	};

	extern CCoroutineOnSuspendHelper g_OnSuspend;
	CFutureCoroutineContext::COnResumeScopeAwaiter fg_OnResume(NFunction::TCFunctionMovable<NException::CExceptionPointer ()> &&_fOnResume);
}

namespace NMib::NFunction
{
	extern template struct TCFunctionMutable<NConcurrency::TCFuture<void> ()>;
}

#include <Mib/Concurrency/Actor/Timer>
#include "Malterlib_Concurrency_CallSafe.h"
#include "Malterlib_Concurrency_BoundActorCall.h"
