// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Coroutine.h"

namespace NMib::NConcurrency
{
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList, bool tf_bDirectCall>
	struct TCActorCall;
	
	class CActor;

	template <typename t_CActor>
	class TCActor;

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CReturnValue>
	struct TCPromise;

	template <typename t_CReturnValue>
	struct TCFuture;
}

namespace NMib::NConcurrency
{
	enum EFutureCoroutineContextFlag
	{
		EFutureCoroutineContextFlag_None = 0
		, EFutureCoroutineContextFlag_CaptureExceptions = DMibBit(0)
#if DMibEnableSafeCheck > 0
		, EFutureCoroutineContextFlag_UnsafeReferenceParameters = DMibBit(1)
		, EFutureCoroutineContextFlag_UnsafeThisPointer = DMibBit(2)
#endif
	};

	struct CFutureCoroutineContext : public CCoroutineHandler
	{
		CFutureCoroutineContext(EFutureCoroutineContextFlag _Flags) noexcept;
		~CFutureCoroutineContext() noexcept;

		inline_always CSuspendNever initial_suspend()
#if DMibEnableSafeCheck <= 0
		 	noexcept
#endif
		;
		inline_always CSuspendAlways final_suspend();

		void f_Suspend();
		void f_Resume();

		CCoroutineHandler *m_pPreviousCoroutineHandler = this;
		EFutureCoroutineContextFlag m_Flags = EFutureCoroutineContextFlag_None;

	private:
#if DMibEnableSafeCheck > 0
		static mint msp_DequeueProcessLocaction_ReferenceParam;
		static CMibCodeAddress msp_DequeueProcessAddress_ReferenceThis;
		static mint msp_WrapDispatchWithReturnLocaction_ReferenceThis;
		static CMibCodeAddress msp_WrapDispatchWithReturnAddress_ReferenceThis;

		inline_never void fp_CheckUnsafeReferenceParams();
		inline_never void fp_CheckUnsafeThisPointer();
#endif
	};
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnValue, typename t_CException = void>
	struct TCRunProtectedHelper
	{
		template <typename tf_FToRun>
		TCAsyncResult<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue>
	struct TCRunProtectedHelper<t_CReturnValue, void>
	{
		template <typename tf_FToRun>
		TCAsyncResult<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CException>
	struct TCRunProtectedHelper<void, t_CException>
	{
		template <typename tf_FToRun>
		TCAsyncResult<void> operator / (tf_FToRun &&_ToRun) const;
	};

	template <>
	struct TCRunProtectedHelper<void, void>
	{
		template <typename tf_FToRun>
		TCAsyncResult<void> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue, typename t_CException = void>
	struct TCRunProtectedFutureHelper
	{
		template <typename tf_FToRun>
		TCFuture<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue>
	struct TCRunProtectedFutureHelper<t_CReturnValue, void>
	{
		template <typename tf_FToRun>
		TCFuture<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CException>
	struct TCRunProtectedFutureHelper<void, t_CException>
	{
		template <typename tf_FToRun>
		TCFuture<void> operator / (tf_FToRun &&_ToRun) const;
	};

	template <>
	struct TCRunProtectedFutureHelper<void, void>
	{
		template <typename tf_FToRun>
		TCFuture<void> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_FFunctor>
	struct TCAllAsyncResultsAreVoid;
	
	template <typename t_CPromise>
	struct TCPromiseReceiveAnyFunctor;

	template <typename t_CReturnValue>
	struct TCPromiseData;

	template <typename t_CReturnType>
	struct TCFutureCoroutineContextValue : public CFutureCoroutineContext
	{
		TCFutureCoroutineContextValue(EFutureCoroutineContextFlag _Flags) noexcept
			: CFutureCoroutineContext{_Flags}
		{
		}

		TCFutureCoroutineContextValue() = default;
		~TCFutureCoroutineContextValue() noexcept;

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
		void return_value(t_CReturnType &&_Value);

		TCPromiseData<t_CReturnType> *m_pPromiseData = nullptr;
	};

	template <>
	struct TCFutureCoroutineContextValue<void> : public CFutureCoroutineContext
	{
		TCFutureCoroutineContextValue(EFutureCoroutineContextFlag _Flags) noexcept
			: CFutureCoroutineContext{_Flags}
		{
		}

		TCFutureCoroutineContextValue() = default;
		~TCFutureCoroutineContextValue() noexcept;

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);

		void return_value(CVoidTag &&_Value);

		TCPromiseData<void> *m_pPromiseData = nullptr;
	};

	template <typename t_CReturnType>
	struct TCFutureCoroutineContext : public TCFutureCoroutineContextValue<t_CReturnType>
	{
		TCFutureCoroutineContext() = default;
		TCFutureCoroutineContext(EFutureCoroutineContextFlag _Flags) noexcept
			: TCFutureCoroutineContextValue<t_CReturnType>(_Flags)
		{
		}

		TCFutureCoroutineContext(TCFutureCoroutineContext &&) = delete;
		~TCFutureCoroutineContext() = default;

		NStorage::TCSharedPointer<TCPromiseData<t_CReturnType>> f_Suspend();

		TCFuture<t_CReturnType> get_return_object();
		void unhandled_exception();
	};

	template <typename t_CReturnType, EFutureCoroutineContextFlag t_Flags = EFutureCoroutineContextFlag_None>
	struct TCFutureCoroutineContextWithFlags : public TCFutureCoroutineContext<t_CReturnType>
	{
		TCFutureCoroutineContextWithFlags() noexcept
			: TCFutureCoroutineContext<t_CReturnType>(t_Flags)
		{
		}
	};

	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception);

	template <typename t_CReturnValue>
	struct TCPromiseData : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		TCAsyncResult<t_CReturnValue> m_Result;
		NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> m_fOnResult;
		NAtomic::TCAtomic<mint> m_OnResultSet;
		TCCoroutineHandle<TCFutureCoroutineContext<t_CReturnValue>> m_Coroutine;

		void fp_ReportNothingSet();
		~TCPromiseData();
		void fp_OnResult();
		void f_SetResult();
		void f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result);

		void f_SetResult(TCPromise<t_CReturnValue> const &_Result);
		void f_SetResult(TCPromise<t_CReturnValue> &_Result);
		void f_SetResult(TCPromise<t_CReturnValue> &&_Result);
		
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> const &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &_Result);
		template <typename tf_CResult>
		void f_SetException(TCAsyncResult<tf_CResult> &&_Result);
		void f_SetCurrentException();
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

	DMibImpErrorSpecificClassDefine(CExceptionCoroutineWrapper, NMib::NException::CException, CExceptionCoroutineData);
#	define DMibErrorCoroutineWrapper(d_Description, d_Specific) DMibImpErrorSpecific(NMib::NConcurrency::CExceptionCoroutineWrapper, d_Description, d_Specific)
#	define DMibErrorInstanceCoroutineWrapper(d_Description, d_Specific) DMibImpExceptionInstanceSpecific(NMib::NWeb::CExceptionCoroutineWrapper, d_Description, d_Specific)

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromiseWithError;

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCFutureWithError;

	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform = void *>
	struct [[nodiscard]] TCFutureAwaiter;

	template <typename t_CReturnValue = void>
	struct [[nodiscard]] TCPromise;

	/// \brief Used to defer the return of a value to allow async programming
	template <typename t_CReturnValue = void>
	struct [[nodiscard]] TCFuture
	{
		struct CNoUnwrapAsyncResult
		{
			TCFuture const *m_pWrapped;
			auto operator co_await();
		};

		TCFuture(TCFuture const &_Other);
		TCFuture(TCFuture &&_Other);
		TCFuture &operator =(TCFuture const &_Other);
		TCFuture &operator =(TCFuture &&_Other);
		TCFuture();

		template <typename tf_CType>
		TCFuture(TCExplicit<tf_CType> &&_Data);
		TCFuture(TCExplicit<void> &&_Data);

		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		TCFuture(tf_CType &&_Exception);

		TCFuture(NException::CExceptionPointer const &_pException);

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		TCFuture(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall);

		TCFuture(TCAsyncResult<t_CReturnValue> const &_Result);
		TCFuture(TCAsyncResult<t_CReturnValue> &&_Result);

		static NPrivate::TCRunProtectedHelper<t_CReturnValue> fs_RunProtectedAsyncResult();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> fs_RunProtectedAsyncResult();

		static NPrivate::TCRunProtectedFutureHelper<t_CReturnValue> fs_RunProtected();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedFutureHelper<t_CReturnValue, tf_CException> fs_RunProtected();

		void f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
		void f_DiscardResult();
		bool f_IsObserved() const;

		bool f_IsCoroutine() const;
		EFutureCoroutineContextFlag f_CoroutineFlags() const;

		TCDispatchedActorCall<t_CReturnValue, true> f_Dispatch() const;
		TCDispatchedActorCall<t_CReturnValue, true> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true);

		auto f_CallSync(fp64 _Timeout) const;

		TCFuture &&f_Move();

		template <typename tf_FResultHandler>
		void operator > (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_CReturnValue>
		void operator > (TCPromise<tf_CReturnValue> const &_Promise) const;

		template <typename tf_CType>
		auto operator + (TCFuture<tf_CType> const &_Other) const;

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall);

		auto operator % (NStr::CStr const &_ErrorString) const -> TCFutureWithError<t_CReturnValue>;

		CNoUnwrapAsyncResult f_Wrap() const;
		auto operator co_await() const;

	public:
		template <typename t_CReturnType>
		friend struct TCFutureCoroutineContext;
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> m_pData;
	};

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCPromise : public TCFuture<t_CReturnValue>
	{
	public:
		TCPromise(TCPromise const &_Other);
		TCPromise(TCPromise &&_Other);
		TCPromise &operator =(TCPromise const &_Other);
		TCPromise &operator =(TCPromise &&_Other);
		TCPromise();

		template <typename tf_CType>
		TCPromise(TCExplicit<tf_CType> &&_Data);
		TCPromise(TCExplicit<void> &&_Data);

		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		TCPromise(tf_CType &&_Exception);

		TCPromise(NException::CExceptionPointer const &_pException);
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
		TCPromise(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall);
		
		TCPromise(TCAsyncResult<t_CReturnValue> const &_Result);
		TCPromise(TCAsyncResult<t_CReturnValue> &&_Result);

		//explicit operator bool () const;
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

		TCPromise &&f_Move();
		TCFuture<t_CReturnValue> f_Future() const;
		TCFuture<t_CReturnValue> &&f_MoveFuture();

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		auto operator % (NStr::CStr const &_ErrorString) const -> TCPromiseWithError<t_CReturnValue>;

	private:
		friend struct TCFuture<t_CReturnValue>;

		TCPromise(TCFuture<t_CReturnValue> const &_Other);
	};

	enum EFutureOption
	{
		EFutureOption_None
		, EFutureOption_CaptureExceptions = DMibBit(0)
		, EFutureOption_AllowReferences = DMibBit(1)
		, EFutureOption_AllowReferencesCaptureExceptions = EFutureOption_CaptureExceptions | EFutureOption_AllowReferences
	};

	template <typename t_CReturnValue = void, EFutureOption t_Options = EFutureOption_None>
	struct [[nodiscard]] TCFutureWithOptions : public TCFuture<t_CReturnValue>
	{
		TCFutureWithOptions(TCFutureWithOptions &&) = default;
		TCFutureWithOptions() = default;

		TCFutureWithOptions &operator = (TCFutureWithOptions &&) = default;

		TCFutureWithOptions(TCFuture<t_CReturnValue> &&_Other)
			: TCFuture<t_CReturnValue>(fg_Move(_Other))
		{
		}

		TCFutureWithOptions(TCFuture<t_CReturnValue> const &_Other)
			: TCFuture<t_CReturnValue>(_Other)
		{
		}

		TCFutureWithOptions &operator = (TCFuture<t_CReturnValue> &&_Other)
		{
			*((TCFuture<t_CReturnValue> *)this) = fg_Move(_Other);
		}

		TCFutureWithOptions &operator = (TCFuture<t_CReturnValue> const &_Other)
		{
			*((TCFuture<t_CReturnValue> *)this) = _Other;
		}
	};

	template <typename t_CReturnValue = void>
	using TCFutureCaptureExceptions = TCFutureWithOptions<t_CReturnValue, EFutureOption_CaptureExceptions>;

	// Warning, when you allow references for parameters in your coroutine, make sure that you don't use the reference after the first suspension point, as it will be out of scope
	// in this case.

	template <typename t_CReturnValue = void>
	using TCFutureAllowReferences = TCFutureWithOptions<t_CReturnValue, EFutureOption_AllowReferences>;

	template <typename t_CReturnValue = void>
	using TCFutureAllowReferencesCaptureExceptions = TCFutureWithOptions<t_CReturnValue, EFutureOption_AllowReferencesCaptureExceptions>;

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
	struct [[nodiscard]] TCFutureWithError : public CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCFutureWithError *m_pWrapped;
			auto operator co_await();
		};

		TCFutureWithError(TCFuture<t_CReturnValue> const &_Future, NStr::CStr const &_Error);

		auto operator co_await();
		CNoUnwrapAsyncResult f_Wrap();

		TCFuture<t_CReturnValue> m_Future;
	};
}

namespace NMibOperators
{
	template <typename t_CReturnValue>
	class TCDisableAutomaticOperators<NMib::NConcurrency::TCPromise<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};

	template <typename t_CReturnValue>
	class TCDisableAutomaticOperators<NMib::NConcurrency::TCFuture<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};

	template <typename t_CReturnValue, NMib::NConcurrency::EFutureOption t_Options>
	class TCDisableAutomaticOperators<NMib::NConcurrency::TCFutureWithOptions<t_CReturnValue, t_Options>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
	{
	};

}

#include "Malterlib_Concurrency_Promise.hpp"
#include <Mib/Concurrency/Actor/Timer>
