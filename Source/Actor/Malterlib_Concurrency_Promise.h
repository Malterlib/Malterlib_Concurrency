// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Coroutine.h"
#include "Malterlib_Concurrency_WeakActor.h"
#include <Mib/Core/CoroutineFlags>

namespace NMib::NConcurrency
{
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool tf_bDirectCall>
	struct TCActorCall;

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

	struct CFutureCoroutineContext : public CCoroutineHandler
	{
		CFutureCoroutineContext(ECoroutineFlag _Flags) noexcept;
		~CFutureCoroutineContext() noexcept;

#if defined(DMibSanitizerEnabled_Address)
		// Workaround address sanitizer bug

		struct CSupendNeverWorkaround
		{
			inline_never bool await_ready() const noexcept;
			inline_never void await_suspend(TCCoroutineHandle<>) const noexcept;
			inline_never void await_resume() const noexcept;
		};

		using CInitialSuspend = CSupendNeverWorkaround;
#else
		using CInitialSuspend = CSuspendNever;
#endif

		CInitialSuspend initial_suspend() noexcept;
		CSuspendNever final_suspend() noexcept;

		template <typename t_CThis, ECoroutineFlag tf_Flags>
		decltype(auto) await_transform(TCFutureForwardThis<t_CThis, tf_Flags> &&_MoveThis)
		{
			DMibFastCheck(m_Flags & ECoroutineFlag_UnsafeThisPointer); // You don't need to move this

#if DMibEnableSafeCheck > 0
			if constexpr ((tf_Flags & (ECoroutineFlag_CaptureExceptions | ECoroutineFlag_AllowReferences)) != ECoroutineFlag_None)
				m_Flags |= tf_Flags;
			if (!(m_Flags & ECoroutineFlag_UnsafeReferenceParameters))
				m_Flags |= ECoroutineFlag_AllowReferences;
#else
			if constexpr ((tf_Flags & (ECoroutineFlag_CaptureExceptions)) != ECoroutineFlag_None)
				m_Flags |= tf_Flags;
#endif
			return fg_Move(_MoveThis);
		}

		CSuspendNever await_transform(ECoroutineFlag _Flags)
		{
			m_Flags |= _Flags;
			return {};
		}

		template <typename t_CAwaitable>
		decltype(auto) await_transform(t_CAwaitable &&_Awaitable)
		{
			return fg_Forward<t_CAwaitable>(_Awaitable);
		}

		void f_Suspend(bool _bAddToActor)
#if DMibEnableSafeCheck <= 0
			noexcept
#endif
		;
		NContainer::TCVector<NFunction::TCFunctionMovable<void (bool _bException)>> f_Resume(bool &o_bAborted, bool _bException);
		virtual void f_Abort() = 0;
		virtual void f_ResumeException() = 0;
		virtual bool f_IsException() = 0;

		CCoroutineHandler *m_pPreviousCoroutineHandler = this;
		ECoroutineFlag m_Flags = ECoroutineFlag_None;

		DMibListLinkDS_Link(CFutureCoroutineContext, m_Link);
		NContainer::TCVector<NFunction::TCFunctionMovable<void (bool _bException)>> m_RestoreScopes;
#if DMibEnableSafeCheck > 0
		bool m_bIsCalledSafely = false;
		static bool ms_bDebugCoroutineSafeCheck;
#endif

	protected:
#if DMibEnableSafeCheck > 0
		inline_never void fp_UpdateSafeCall(bool _bSafeCall);

		inline_never void fp_CheckUnsafeReferenceParams();
		inline_never void fp_CheckUnsafeThisPointer();
#endif
	};
}

namespace NMib::NConcurrency::NPrivate
{
	struct CDiscardResultFunctor;

	template <typename t_CReturnValue, typename t_CException = void>
	struct TCRunProtectedHelper
	{
		template <typename tf_FToRun>
		TCAsyncResult<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue, typename t_CException = void>
	struct TCRunProtectedFutureHelper
	{
		using CReturn = TCFuture<t_CReturnValue>;

		template <typename tf_FToRun, typename ...tfp_CParams>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator () (tf_FToRun &&_ToRun, tfp_CParams && ...p_Params) const;

		template <typename tf_FToRun>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_FFunctor>
	struct TCAllAsyncResultsAreVoid;

	template <typename t_CPromise>
	struct TCPromiseReceiveAnyFunctor;

	template <typename t_CReturnValue>
	struct TCPromiseData;

	template <typename t_CReturnType>
	struct TCFutureCoroutineKeepAlive
	{
		TCFutureCoroutineKeepAlive(TCActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAlive(TCWeakActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAlive(TCFutureCoroutineKeepAlive &&) = default;
		TCFutureCoroutineKeepAlive(TCFutureCoroutineKeepAlive const &) = delete;

		bool f_HasValidCoroutine() const;

		TCActor<CActor> f_MoveActor();

	private:
		TCActor<CActor> mp_Actor;
		TCWeakActor<CActor> mp_WeakActor;
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> mp_pPromiseData;
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineKeepAliveImplicit
	{
		TCFutureCoroutineKeepAliveImplicit(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData);
		TCFutureCoroutineKeepAliveImplicit(TCFutureCoroutineKeepAliveImplicit &&) = default;
		TCFutureCoroutineKeepAliveImplicit(TCFutureCoroutineKeepAliveImplicit const &) = delete;

		bool f_HasValidCoroutine() const;

	private:
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> mp_pPromiseData;
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContextShared : public CFutureCoroutineContext
	{
		using CReturnType = t_CReturnType;

		TCFutureCoroutineContextShared(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: CFutureCoroutineContext(_Flags)
		{
		}

		TCFutureCoroutineContextShared(TCFutureCoroutineContextShared &&) = delete;
		~TCFutureCoroutineContextShared() noexcept;

		TCFutureCoroutineKeepAlive<t_CReturnType> f_KeepAlive(TCActor<> &&_Actor);
		TCFutureCoroutineKeepAliveImplicit<t_CReturnType> f_KeepAliveImplicit();
		void f_Abort() override;
		void f_ResumeException() override;
		bool f_IsException() override;

		mark_no_coroutine_debug TCFuture<t_CReturnType> get_return_object();
		void unhandled_exception();

#if DMibEnableSafeCheck > 0
		void f_SetOwner(TCWeakActor<> const &_CoroutineOwner);
#endif

		TCPromiseData<t_CReturnType> *m_pPromiseData = nullptr;

	protected:
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> fp_GetReturnObject();
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContext : public TCFutureCoroutineContextShared<t_CReturnType>
	{
		TCFutureCoroutineContext(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: TCFutureCoroutineContextShared<t_CReturnType>(_Flags)
		{
		}

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
		void return_value(t_CReturnType &&_Value);
	};

	template <>
	struct TCFutureCoroutineContext<void> : public TCFutureCoroutineContextShared<void>
	{
		TCFutureCoroutineContext(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept
			: TCFutureCoroutineContextShared<void>(_Flags)
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
	};

	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception, NStr::CStr const &_CallStack = {});

	enum EConsumeFutureOnResultSetFlag : uint32
	{
		EConsumeFutureOnResultSetFlag_None = 0
		, EConsumeFutureOnResultSetFlag_ResetSafeCall = DMibBit(0)
	};

	template <typename tf_CReturnValue>
	NFunction::TCFunctionMovable<void (TCAsyncResult<tf_CReturnValue> &&_AsyncResult)> fg_ConsumeFutureOnResultSet
		(
#if DMibEnableSafeCheck > 0
		 	void const *_pConsumedBy
			, EConsumeFutureOnResultSetFlag _Flags
#endif
		)
	;

#if DMibConfig_Concurrency_DebugFutures
	struct CPromiseDataBase : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		CPromiseDataBase(NStr::CStr const &_Name);
		virtual ~CPromiseDataBase();

		DMibListLinkDS_Link(CPromiseDataBase, m_Link);
		NStr::CStr m_FutureTypeName;
	};
#else
	using CPromiseDataBase = NStorage::TCSharedPointerIntrusiveBase<>;
#endif

	template <typename t_CReturnValue>
	struct TCPromiseData : public CPromiseDataBase
	{
		TCPromiseData();
		~TCPromiseData();

		void f_Reset();

		void fp_ReportNothingSet();
		void f_OnResult();
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
		void f_SetCurrentExceptionNoReport();

		NAtomic::EMemoryOrder f_MemoryOrder(NAtomic::EMemoryOrder _Default = NAtomic::EMemoryOrder_SequentiallyConsistent) const;

		TCAsyncResult<t_CReturnValue> m_Result;
		NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> m_fOnResult;
		NAtomic::TCAtomic<mint> m_OnResultSet;
		TCCoroutineHandle<TCFutureCoroutineContextShared<t_CReturnValue>> m_Coroutine;
		bool m_bOnResultSetAtInit = false;
		bool m_bIsGenerator = false;
		bool m_bIsCoroutine = false;
		bool m_bPendingResult = false;
#if DMibEnableSafeCheck > 0
		TCWeakActor<> m_CoroutineOwner;
		bool m_bFutureGotten = false;
#endif
#if DMibConfig_Concurrency_DebugUnobservedException
		NException::CCallstack m_Callstack;
#endif
	};
}

template <typename t_CReturnValue>
struct NMib::NStorage::TCHasIntrusiveRefcount<NMib::NConcurrency::NPrivate::TCPromiseData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CReturnValue>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::NPrivate::TCPromiseData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{
};

extern "C" void fg_MalterlibConcurrency_TCFutureFunctionEnter(void *_pThisFunction, void *_pCallSite);

namespace NMib::NConcurrency
{
	struct CExceptionCoroutineData
	{
		CExceptionCoroutineData() = default;
		CExceptionCoroutineData(NException::CExceptionPointer &&_pException)
			: m_pException(fg_Move(_pException))
		{
		}

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream)
		{
			DMibPDebugBreak; // Not valid for streaming
		}

		NException::CExceptionPointer m_pException;
	};

	DMibImpErrorSpecificClassDefine(CExceptionCoroutineWrapper, NMib::NException::CExceptionBase, CExceptionCoroutineData);
#	define DMibErrorCoroutineWrapper(d_Description, d_Specific) DMibImpErrorSpecific(NMib::NConcurrency::CExceptionCoroutineWrapper, d_Description, d_Specific, false)
#	define DMibErrorInstanceCoroutineWrapper(d_Description, d_Specific) DMibImpExceptionInstanceSpecific(NMib::NConcurrency::CExceptionCoroutineWrapper, d_Description, d_Specific, false)

	template <typename t_CReturnValue>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCPromiseWithError;

	template <typename t_CReturnValue>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFutureWithError;

	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform = void *>
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
	}

	/// \brief Used to defer the return of a value to allow async programming
	template <typename t_CReturnValue = void>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] DMibConcurrencyInstrumentFunctionEnter TCFuture
	{
		struct CNoUnwrapAsyncResult
		{
			TCFuture *m_pWrapped;
			auto operator co_await() &&;
		};

		TCFuture();
		TCFuture(TCFuture &&_Other);
		TCFuture &operator =(TCFuture &&_Other);

		static NPrivate::TCRunProtectedHelper<t_CReturnValue> fs_RunProtectedAsyncResult();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> fs_RunProtectedAsyncResult();

		static NPrivate::TCRunProtectedFutureHelper<t_CReturnValue> fs_RunProtected();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedFutureHelper<t_CReturnValue, tf_CException> fs_RunProtected();

		bool f_OnResultSetOrAvailable(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
		void f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
		void f_DiscardResult();
		bool f_ObserveIfAvailable();
		bool f_IsObserved() const;
		TCAsyncResult<t_CReturnValue> &&f_MoveResult();

		bool f_IsCoroutine() const;
		ECoroutineFlag f_CoroutineFlags() const;

		auto f_Dispatch() &&;
		TCFuture<t_CReturnValue> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true) &&;

		auto f_CallSync() &&;
		auto f_CallSync(fp64 _Timeout) &&;
		auto f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout = -1.0) &&;

		TCFuture &&f_Move();

		template <typename tf_FResultHandler>
		void operator > (tf_FResultHandler &&_fResultHandler) &&;
		void operator > (TCActorResultCall<TCActor<CConcurrentActor>, NPrivate::CDiscardResultFunctor> &&_fResultHandler) &&;

		template <typename tf_CReturnValue>
		void operator > (TCPromise<tf_CReturnValue> const &_Promise) &&;

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> &&_Other) &&;

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall) &&;

		auto operator % (NStr::CStr const &_ErrorString) && -> TCFutureWithError<t_CReturnValue>;

		CNoUnwrapAsyncResult f_Wrap() &&;
		auto operator co_await() &&;

		bool f_HasData(void const *_pData) const;

	private:
		friend struct TCPromise<t_CReturnValue>;
		template <typename t_CReturnType>
		friend struct NPrivate::TCFutureCoroutineContextShared;
		template <typename t_CReturnType>
		friend struct NPrivate::TCAsyncGeneratorCoroutineContext;

		TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &_pData);
		TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData);

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

	template <typename t_CReturnValue = void>
	using TCUnsafeFuture = TCFutureWithFlags<t_CReturnValue, ECoroutineFlag_AllowReferences>;

	using CUnsafeFuture = TCUnsafeFuture<void>;

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromise
	{
	public:
		TCPromise(TCPromise const &_Other);
		TCPromise(TCPromise &&_Other);
		TCPromise &operator =(TCPromise const &_Other);
		TCPromise &operator =(TCPromise &&_Other);
		TCPromise();

		bool f_IsSet() const;
		void f_SetResult() const;
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result) const;
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result) const;
		template <typename tf_CException, typename tf_CResult>
		void f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const;
		void f_SetCurrentException() const;
		auto f_ReceiveAny() const -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise>;
		template <typename tf_FOnEmpty>
		void f_Abandon(tf_FOnEmpty &&_fOnEmpty);
		TCAsyncResult<t_CReturnValue> &&f_MoveResult();

		TCPromise &&f_Move();
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_Future() const;
		mark_no_coroutine_debug TCFuture<t_CReturnValue> f_MoveFuture();

		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (CVoidTag const &);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (TCFuture<t_CReturnValue> &&_Future);
		template <typename tf_CResult, TCEnableIfType<!NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CResult>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (tf_CResult &&_Result);
		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (tf_CType &&_Exception);

		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer &&_pException);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer &_pException);
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (NException::CExceptionPointer const &_pException);

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator <<= (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall);


		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		auto operator % (NStr::CStr const &_ErrorString) const -> TCPromiseWithError<t_CReturnValue>;

	private:
		friend struct TCFuture<t_CReturnValue>;

		TCPromise(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &_pData);

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> mp_pData;
	};

	struct CActorWithErrorBase
	{
		NStr::CStr m_Error;
	protected:
		NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fp_GetTransformer();
	};

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromiseWithError : public CActorWithErrorBase
	{
		TCPromiseWithError(TCPromise<t_CReturnValue> const &_Promise, NStr::CStr const &_Error);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCPromise<t_CReturnValue> m_Promise;
	};

	template <typename t_CReturnValue>
	struct [[nodiscard("You need to co_await or forward the result to a functor with > operator")]] TCFutureWithError : public CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCFutureWithError *m_pWrapped;
			auto operator co_await() &&;
		};

		TCFutureWithError(TCFuture<t_CReturnValue> &&_Future, NStr::CStr const &_Error);

		auto operator co_await() &&;
		CNoUnwrapAsyncResult f_Wrap() &&;

		TCFuture<t_CReturnValue> m_Future;
	};

	using FUnitVoidFutureFunction = NFunction::TCFunctionMovable<TCFuture<void> ()>;

	struct CFutureMakeHelper
	{
		template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool t_bDirectCall>
		auto operator <<= (TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList, t_bDirectCall> &&_ActorCall)
		{
			return TCPromise<typename TCActorCall<t_CActor, t_CFunctor, t_CParams, t_CTypeList, t_bDirectCall>::CReturnType>() <<= fg_Move(_ActorCall);
		}
	};

	extern CFutureMakeHelper g_Future;

	struct CCoroutineOnSuspendScope final : public CCoroutineThreadLocalHandler
	{
		CCoroutineOnSuspendScope(NFunction::TCFunctionMovable<void ()> &&_fOnSuspend);
		~CCoroutineOnSuspendScope();
		void f_Suspend() override;
		void f_Resume() override;
		void operator ()();

	private:
		NFunction::TCFunctionMovable<void ()> m_fOnSuspend;
	};

	struct CCoroutineOnResumeScope final : public CCoroutineThreadLocalHandler
	{
		CCoroutineOnResumeScope(NFunction::TCFunctionMovable<void ()> &&_fOnResume);
		~CCoroutineOnResumeScope();
		void f_Suspend() override;
		void f_Resume() override;

		void operator ()();

	private:
		NFunction::TCFunctionMovable<void ()> m_fOnResume;
	};

	struct CCoroutineOnSuspendHelper
	{
		CCoroutineOnSuspendScope operator / (NFunction::TCFunctionMovable<void ()> &&_fOnSuspend) const;
	};

	struct CCoroutineOnResumeHelper
	{
		CCoroutineOnResumeScope operator / (NFunction::TCFunctionMovable<void ()> &&_fOnResume) const;
	};

	extern CCoroutineOnSuspendHelper g_OnSuspend;
	extern CCoroutineOnResumeHelper g_OnResume;
}

namespace NMib::NFunction
{
	extern template struct TCFunctionMutable<NConcurrency::TCFuture<void> ()>;
}

#include <Mib/Concurrency/Actor/Timer>
#include "Malterlib_Concurrency_CallSafe.h"
