// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_Coroutine.h"

namespace NMib::NConcurrency
{
	template <typename t_CActor, typename t_CFunctor, typename t_CParams, typename t_CTypeList>
	struct TCActorCall;
	
	class CActor;

	template <typename t_CActor>
	class TCActor;

	template <typename t_CActor>
	class TCWeakActor;

	template <typename t_CReturnValue>
	struct TCContinuation;
		
	template <typename t_CReturn>
	using TCDispatchedActorCall =
		TCActorCall
		<
			TCActor<CActor>
			, TCContinuation<t_CReturn> (CActor::*)(NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()> &&)
			, NStorage::TCTuple<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>>
			, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>>
		>
	;

	template <typename t_CReturn>
	using TCDispatchedWeakActorCall =
		TCActorCall
		<
			TCWeakActor<CActor>
			, TCContinuation<t_CReturn> (CActor::*)(NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()> &&)
			, NStorage::TCTuple<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>>
			, NMeta::TCTypeList<NFunction::TCFunctionMovable<TCContinuation<t_CReturn> ()>>
		>
	;
}

namespace NMib::NConcurrency
{
	enum EContinuationCoroutineContextFlag
	{
		EContinuationCoroutineContextFlag_None = 0
		, EContinuationCoroutineContextFlag_CaptureExceptions = DMibBit(0)
#if DMibEnableSafeCheck > 0
		, EContinuationCoroutineContextFlag_UnsafeReferenceParameters = DMibBit(1)
		, EContinuationCoroutineContextFlag_UnsafeThisPointer = DMibBit(2)
#endif
	};

	struct CContinuationCoroutineContext : public CCoroutineHandler
	{
		CContinuationCoroutineContext(EContinuationCoroutineContextFlag _Flags) noexcept;
		~CContinuationCoroutineContext() noexcept;

		inline_always CSuspendNever initial_suspend()
#if DMibEnableSafeCheck <= 0
		 	noexcept
#endif
		;
		inline_always CSuspendAlways final_suspend();

		void f_Suspend();
		void f_Resume();

		CCoroutineHandler *m_pPreviousCoroutineHandler = this;
		EContinuationCoroutineContextFlag m_Flags = EContinuationCoroutineContextFlag_None;

	private:
#if DMibEnableSafeCheck > 0
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
		TCContinuation<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CReturnValue>
	struct TCRunProtectedHelper<t_CReturnValue, void>
	{
		template <typename tf_FToRun>
		TCContinuation<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_CException>
	struct TCRunProtectedHelper<void, t_CException>
	{
		template <typename tf_FToRun>
		TCContinuation<void> operator / (tf_FToRun &&_ToRun) const;
	};
	
	template <>
	struct TCRunProtectedHelper<void, void>
	{
		template <typename tf_FToRun>
		TCContinuation<void> operator / (tf_FToRun &&_ToRun) const;
	};

	template <typename t_FFunctor>
	struct TCAllAsyncResultsAreVoid;
	
	template <typename t_CContinuation>
	struct TCContinuationReceiveAnyFunctor;

	template <typename t_CReturnValue>
	struct TCContinuationData;

	template <typename t_CReturnType>
	struct TCContinuationCoroutineContextValue : public CContinuationCoroutineContext
	{
		TCContinuationCoroutineContextValue(EContinuationCoroutineContextFlag _Flags) noexcept
			: CContinuationCoroutineContext{_Flags}
		{
		}

		TCContinuationCoroutineContextValue() = default;
		~TCContinuationCoroutineContextValue() noexcept;

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
		void return_value(t_CReturnType &&_Value);

		TCContinuationData<t_CReturnType> *m_pContinuationData = nullptr;
	};

	template <>
	struct TCContinuationCoroutineContextValue<void> : public CContinuationCoroutineContext
	{
		TCContinuationCoroutineContextValue(EContinuationCoroutineContextFlag _Flags) noexcept
			: CContinuationCoroutineContext{_Flags}
		{
		}

		TCContinuationCoroutineContextValue() = default;
		~TCContinuationCoroutineContextValue() noexcept;

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);

		struct CVoidSentinel
		{
		};

		void return_value(CVoidSentinel &&_Value);

		TCContinuationData<void> *m_pContinuationData = nullptr;
	};

	template <typename t_CReturnType>
	struct TCContinuationCoroutineContext : public TCContinuationCoroutineContextValue<t_CReturnType>
	{
		TCContinuationCoroutineContext() = default;
		TCContinuationCoroutineContext(EContinuationCoroutineContextFlag _Flags) noexcept
			: TCContinuationCoroutineContextValue<t_CReturnType>(_Flags)
		{
		}

		TCContinuationCoroutineContext(TCContinuationCoroutineContext &&) = delete;
		~TCContinuationCoroutineContext() = default;

		NStorage::TCSharedPointer<TCContinuationData<t_CReturnType>> f_Suspend();

		TCContinuation<t_CReturnType> get_return_object();
		void unhandled_exception();
	};

	template <typename t_CReturnType, EContinuationCoroutineContextFlag t_Flags = EContinuationCoroutineContextFlag_None>
	struct TCContinuationCoroutineContextWithFlags : public TCContinuationCoroutineContext<t_CReturnType>
	{
		TCContinuationCoroutineContextWithFlags() noexcept
			: TCContinuationCoroutineContext<t_CReturnType>(t_Flags)
		{
		}
	};

	void fg_ReportUnobservedException(NException::CExceptionPointer const &_Exception);

	template <typename t_CReturnValue>
	struct TCContinuationData : public NStorage::TCSharedPointerIntrusiveBase<>
	{
		TCAsyncResult<t_CReturnValue> m_Result;
		NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> m_fOnResult;
		NAtomic::TCAtomic<mint> m_OnResultSet;
		TCCoroutineHandle<TCContinuationCoroutineContext<t_CReturnValue>> m_Coroutine;

		void fp_ReportNothingSet();
		~TCContinuationData();
		void fp_OnResult();
		void f_SetResult();
		void f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result);
		void f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result);

		void f_SetResult(TCContinuation<t_CReturnValue> const &_Result);
		void f_SetResult(TCContinuation<t_CReturnValue> &_Result);
		void f_SetResult(TCContinuation<t_CReturnValue> &&_Result);
		
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
struct NMib::NStorage::TCHasIntrusiveRefcount<NMib::NConcurrency::NPrivate::TCContinuationData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, true>
{
};

template <typename t_CReturnValue>
struct NMib::NTraits::TCHasVirtualDestructor<NMib::NConcurrency::NPrivate::TCContinuationData<t_CReturnValue>> : public NMib::NTraits::TCCompileTimeConstant<bool, false>
{ 
};

namespace NMib::NConcurrency
{
	struct CExceptionCoroutineData
	{
		CExceptionCoroutineData() = default;
		CExceptionCoroutineData(CExceptionPointer &&_pException)
			: m_pException(fg_Move(_pException))
		{
		}

		template <typename tf_CStream>
		void f_Stream(tf_CStream &_Stream)
		{
			DMibPDebugBreak; // Not valid for streaming
		}

		CExceptionPointer m_pException;
	};

	DMibImpErrorSpecificClassDefine(CExceptionCoroutineWrapper, NMib::NException::CException, CExceptionCoroutineData);
#	define DMibErrorCoroutineWrapper(d_Description, d_Specific) DMibImpErrorSpecific(NMib::NConcurrency::CExceptionCoroutineWrapper, d_Description, d_Specific)
#	define DMibErrorInstanceCoroutineWrapper(d_Description, d_Specific) DMibImpExceptionInstanceSpecific(NMib::NWeb::CExceptionCoroutineWrapper, d_Description, d_Specific)

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCContinuationWithError;

	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform = void *>
	struct [[nodiscard]] TCContinuationAwaiter;

	/// \brief Used to defer the return of a value to allow async programming
	template <typename t_CReturnValue = void>
	struct [[nodiscard]] TCContinuation
	{
	public:
		struct CNoUnwrapAsyncResult
		{
			TCContinuation const *m_pWrapped;
			auto operator co_await();
		};

		TCContinuation(TCContinuation const &_Other);
		TCContinuation(TCContinuation &&_Other);
		TCContinuation &operator =(TCContinuation const &_Other);
		TCContinuation &operator =(TCContinuation &&_Other);
		TCContinuation();

		template <typename tf_CType>
		TCContinuation(TCExplicit<tf_CType> &&_Data);
		TCContinuation(TCExplicit<void> &&_Data);

		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> * = nullptr>
		TCContinuation(tf_CType &&_Exception);

		TCContinuation(CExceptionPointer const &_pException);
		
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		TCContinuation(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall);
		
		TCContinuation(TCAsyncResult<t_CReturnValue> const &_Result);
		TCContinuation(TCAsyncResult<t_CReturnValue> &&_Result);

		static NPrivate::TCRunProtectedHelper<t_CReturnValue> fs_RunProtected();
		template <typename tf_CException>
		static NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> fs_RunProtected();

		explicit operator bool () const;
		bool f_IsSet() const;
		void f_SetResult() const;
		template <typename tf_CResult>
		void f_SetResult(tf_CResult &&_Result) const;
		template <typename tf_CResult>
		void f_SetException(tf_CResult &&_Result) const;
		template <typename tf_CException, typename tf_CResult>
		void f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const;
		void f_SetCurrentException() const;
		auto f_ReceiveAny() const -> NPrivate::TCContinuationReceiveAnyFunctor<TCContinuation>;

		void f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult);
		void f_DiscardResult();
		bool f_IsCoroutine() const;
		EContinuationCoroutineContextFlag f_CoroutineFlags() const;

		TCDispatchedActorCall<t_CReturnValue> f_Dispatch() const;
		TCDispatchedActorCall<t_CReturnValue> f_Timeout(fp64 _Timeout, NStr::CStr const &_TimeoutMessage, bool _bFireAtExit = true); // #include <Mib/Concurrency/Actor/Timer> to use

		auto f_CallSync(fp64 _Timeout) const;
		
		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		auto operator % (NStr::CStr const &_ErrorString) const -> TCContinuationWithError<t_CReturnValue>;

		template <typename tf_FResultHandler>
		void operator > (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_CReturnValue>
		void operator > (TCContinuation<tf_CReturnValue> const &_Continuation) const;

		template <typename tf_CType>
		auto operator + (TCContinuation<tf_CType> const &_Other) const;

		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		auto operator + (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall);

		CNoUnwrapAsyncResult f_Wrap() const;
		auto operator co_await() const;

		TCContinuation &&f_Move();

	public:
		template <typename t_CReturnType>
		friend struct TCContinuationCoroutineContext;

		NStorage::TCSharedPointer<NPrivate::TCContinuationData<t_CReturnValue>> m_pData;
	};

	enum EContinuationOption
	{
		EContinuationOption_None
		, EContinuationOption_CaptureExceptions = DMibBit(0)
		, EContinuationOption_AllowReferences = DMibBit(1)
		, EContinuationOption_AllowReferencesCaptureExceptions = EContinuationOption_CaptureExceptions | EContinuationOption_AllowReferences
	};

	template <typename t_CReturnValue = void, EContinuationOption t_Options = EContinuationOption_None>
	struct [[nodiscard]] TCContinuationWithOptions : public TCContinuation<t_CReturnValue>
	{
		TCContinuationWithOptions(TCContinuationWithOptions &&) = default;
		TCContinuationWithOptions() = default;

		TCContinuationWithOptions &operator = (TCContinuationWithOptions &&) = default;

		TCContinuationWithOptions(TCContinuation<t_CReturnValue> &&_Other)
			: TCContinuation<t_CReturnValue>(fg_Move(_Other))
		{
		}

		TCContinuationWithOptions(TCContinuation<t_CReturnValue> const &_Other)
			: TCContinuation<t_CReturnValue>(_Other)
		{
		}

		TCContinuationWithOptions &operator = (TCContinuation<t_CReturnValue> &&_Other)
		{
			*((TCContinuation<t_CReturnValue> *)this) = fg_Move(_Other);
		}

		TCContinuationWithOptions &operator = (TCContinuation<t_CReturnValue> const &_Other)
		{
			*((TCContinuation<t_CReturnValue> *)this) = _Other;
		}
	};

	template <typename t_CReturnValue = void>
	using TCContinuationCaptureExceptions = TCContinuationWithOptions<t_CReturnValue, EContinuationOption_CaptureExceptions>;

	// Warning, when you allow references for parameters in your coroutine, make sure that you don't use the reference after the first suspension point, as it will be out of scope
	// in this case.

	template <typename t_CReturnValue = void>
	using TCContinuationAllowReferences = TCContinuationWithOptions<t_CReturnValue, EContinuationOption_AllowReferences>;

	template <typename t_CReturnValue = void>
	using TCContinuationAllowReferencesCaptureExceptions = TCContinuationWithOptions<t_CReturnValue, EContinuationOption_AllowReferencesCaptureExceptions>;

	struct CActorWithErrorBase
	{
		NStr::CStr m_Error;
	protected:
		NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> fp_GetTransformer();
	};

	template <typename t_CReturnValue>
	struct [[nodiscard]] TCContinuationWithError : public CActorWithErrorBase
	{
		struct CNoUnwrapAsyncResult
		{
			TCContinuationWithError *m_pWrapped;
			auto operator co_await();
		};

		TCContinuationWithError(TCContinuation<t_CReturnValue> const &_Continuation, NStr::CStr const &_Error);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		auto operator co_await();
		CNoUnwrapAsyncResult f_Wrap();

		TCContinuation<t_CReturnValue> m_Continuation;
	};
}

#include "Malterlib_Concurrency_Continuation.hpp"
