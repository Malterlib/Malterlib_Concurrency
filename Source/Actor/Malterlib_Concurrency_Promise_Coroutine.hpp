// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always CFutureCoroutineContext::CFutureCoroutineContext(EFutureCoroutineContextFlag _Flags) noexcept
		: m_Flags(_Flags)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;
	}

	inline_always CFutureCoroutineContext::~CFutureCoroutineContext() noexcept
	{
		if (m_pPreviousCoroutineHandler != this)
		{
			auto &ThreadLocal = **g_SystemThreadLocal;
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
			ThreadLocal.m_pCurrentCoroutineHandler = m_pPreviousCoroutineHandler;
		}
	}

#if DMibEnableSafeCheck > 0
	inline_never
#endif
	CSuspendNever CFutureCoroutineContext::initial_suspend()
#if DMibEnableSafeCheck <= 0
	 	noexcept
#endif
	{
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = **g_SystemThreadLocal;
		
		if (m_Flags & EFutureCoroutineContextFlag_UnsafeThisPointer)
			fp_CheckUnsafeThisPointer();
		else if (m_Flags & EFutureCoroutineContextFlag_UnsafeReferenceParameters)
			fp_CheckUnsafeReferenceParams();

		ThreadLocal.m_bExpectCoroutineCall = false;
#endif

		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);

		return {};
	}

	CSuspendAlways CFutureCoroutineContext::final_suspend()
	{
		if (m_pPreviousCoroutineHandler != this)
		{
			auto &ThreadLocal = **g_SystemThreadLocal;
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
			ThreadLocal.m_pCurrentCoroutineHandler = m_pPreviousCoroutineHandler;
			m_pPreviousCoroutineHandler = this;
		}
		return {};
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnType>
	TCFutureCoroutineContextValue<t_CReturnType>::~TCFutureCoroutineContextValue() noexcept
	{
		if (m_pPromiseData)
			m_pPromiseData->m_Coroutine = nullptr;
	}

	inline_always TCFutureCoroutineContextValue<void>::~TCFutureCoroutineContextValue() noexcept
	{
		if (m_pPromiseData)
			m_pPromiseData->m_Coroutine = nullptr;
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContextValue<t_CReturnType>::return_value(t_CReturnType &&_Value)
	{
		m_pPromiseData->f_SetResult(fg_Move(_Value));
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCFutureCoroutineContextValue<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReference<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			m_pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value || NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
			m_pPromiseData->f_SetException(fg_Forward<tf_CReturnType>(_Value));
		else
			m_pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename tf_CReturnType>
	void TCFutureCoroutineContextValue<void>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReference<tf_CReturnType>::CType;
		static_assert
			(
			 	NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value
			 	|| NException::TCIsExcption<CReturnNoReference>::mc_Value
			 	|| NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value
			 	, "You can only return exceptions or async results"
			)
		;

		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value)
			m_pPromiseData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else
			m_pPromiseData->f_SetException(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename t_CReturnType>
	TCFuture<t_CReturnType> TCFutureCoroutineContext<t_CReturnType>::get_return_object()
	{
		TCFuture<t_CReturnType> Future;

		auto *pPromiseData = Future.m_pData.f_Get();
		pPromiseData->m_Coroutine = TCCoroutineHandle<TCFutureCoroutineContext>::from_promise(*this);

		this->m_pPromiseData = pPromiseData;

		return Future;
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContext<t_CReturnType>::unhandled_exception()
	{
		if (this->m_Flags & EFutureCoroutineContextFlag_CaptureExceptions)
		{
			try
			{
				throw;
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetException(_WrappedException.f_GetSpecific().m_pException);
			}
			catch (...)
			{
				this->m_pPromiseData->f_SetCurrentException();
			}
		}
		else
		{
			try
			{
				throw;
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetException(_WrappedException.f_GetSpecific().m_pException);
			}
			// All other exceptionss falls through and crashes the application
		}
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<TCPromiseData<t_CReturnType>> TCFutureCoroutineContext<t_CReturnType>::f_Suspend()
	{
		CFutureCoroutineContext::f_Suspend();
		return fg_Explicit(this->m_pPromiseData);
	}
}

namespace NMib::NConcurrency
{
	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCFutureAwaiter
	{
		TCFutureAwaiter(TCFuture<t_CReturnType> const &_Future, t_FExceptionTransform &&_fExceptionTransform)
			: mp_Future(_Future)
			, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
		{
		}

		bool await_ready() const noexcept
		{
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			DMibFastCheck(fg_CurrentActor());

			auto &CoroutineContext = _Handle.promise();

			auto pKeepAlive = CoroutineContext.f_Suspend();
			auto CurrentActor = fg_CurrentActor();

			mp_Future.f_OnResultSet
				(
					[CurrentActor = fg_Move(CurrentActor), pKeepAlive = fg_Move(pKeepAlive), _Handle](TCAsyncResult<t_CReturnType> &&) mutable
					{
						CurrentActor->f_QueueProcess
							(
								[CurrentActor, pKeepAlive = fg_Move(pKeepAlive), _Handle]() mutable
								{
									auto &CoroutineContext = _Handle.promise();
									CCurrentActorScope CurrentActorScope(CurrentActor);
									CoroutineContext.f_Resume();
									_Handle.resume();
								}
							)
						;
					}
				)
			;

			return true;
		}

		auto await_resume() noexcept(!t_bUnwrap)
		{
			auto &FutureResult = mp_Future.m_pData->m_Result;
			if constexpr (t_bUnwrap)
				return fg_UnwrapCoroutineAsyncResult(fg_Move(FutureResult), mp_fExceptionTransform);
			else
			{
				if constexpr (NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
					return fg_Move(FutureResult);
				else
				{
					if (FutureResult)
						return fg_Move(FutureResult);
					else
					{
						TCAsyncResult<t_CReturnType> Result;
						Result.f_SetException(mp_fExceptionTransform(fg_Move(FutureResult.f_GetException())));
						return Result;
					}
				}
			}
		}

	private:
		TCFuture<t_CReturnType> mp_Future;
		/*[[no_unique_address]]*/ t_FExceptionTransform mp_fExceptionTransform;
	};

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator co_await() const
	{
		return TCFutureAwaiter<t_CReturnValue, true>(*this, nullptr);
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCFutureAwaiter<t_CReturnValue, false>(*m_pWrapped, nullptr);
	}

	template <typename t_CReturnValue>
	TCFutureWithError<t_CReturnValue>::TCFutureWithError(TCFuture<t_CReturnValue> const &_Future, NStr::CStr const &_Error)
		: CActorWithErrorBase{_Error}
		, m_Future(_Future)
	{
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::f_Wrap() -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::operator co_await()
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Future
			 	, this->fp_GetTransformer()
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_pWrapped->m_Future
			 	, m_pWrapped->fp_GetTransformer()
			)
		;
	}
}

namespace NMib::NConcurrency::NPrivate
{
#if DMibEnableSafeCheck > 0
	template <typename tfp_CThis, bool t_bIsComplete = NTraits::TCIsComplete<tfp_CThis>::mc_Value>
	struct TCCoroutineContextFlagsFromParams_This
	{
		static constexpr NMib::NConcurrency::EFutureCoroutineContextFlag mc_Value = NMib::NConcurrency::EFutureCoroutineContextFlag_UnsafeThisPointer; // Assume lambda
	};

	template <typename tfp_CThis>
	struct TCCoroutineContextFlagsFromParams_This<tfp_CThis, true>
	{
		static constexpr NMib::NConcurrency::EFutureCoroutineContextFlag mc_Value
			=
			(
			 	NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActor>::mc_Value
			 	|| NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActorInternal>::mc_Value
			)
			? NMib::NConcurrency::EFutureCoroutineContextFlag_None
			: NMib::NConcurrency::EFutureCoroutineContextFlag_UnsafeThisPointer
		;
	};

	template <typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams
	{
		static constexpr NMib::NConcurrency::EFutureCoroutineContextFlag mc_Value = NMib::NConcurrency::EFutureCoroutineContextFlag_None;
	};

#ifdef DCompiler_MSVC_Workaround
	template <typename ...tfp_CParams>
	bool constexpr fg_AnyIsReference()
	{
		return (... || NMib::NTraits::TCIsReference<tfp_CParams>::mc_Value);
	}

	template <typename tp_CFirstParam, typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams<tp_CFirstParam, tp_CParams...>
	{
		static constexpr NMib::NConcurrency::EFutureCoroutineContextFlag mc_Value =
			(
				fg_AnyIsReference<tp_CParams...>()
				? NMib::NConcurrency::EFutureCoroutineContextFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::EFutureCoroutineContextFlag_None
			)
			| TCCoroutineContextFlagsFromParams_This<typename TCChooseType<NTraits::TCIsPointer<tp_CFirstParam>::mc_Value, typename NTraits::TCAddLValueReference<typename NTraits::TCRemovePointer<tp_CFirstParam>::CType>::CType, tp_CFirstParam>::CType>::mc_Value;
		;
	};
#else
	template <typename tp_CFirstParam, typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams<tp_CFirstParam, tp_CParams...>
	{
		static constexpr NMib::NConcurrency::EFutureCoroutineContextFlag mc_Value =
			(
				(... || NMib::NTraits::TCIsReference<tp_CParams>::mc_Value)
				? NMib::NConcurrency::EFutureCoroutineContextFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::EFutureCoroutineContextFlag_None
			)
			| TCCoroutineContextFlagsFromParams_This<tp_CFirstParam>::mc_Value;
		;
	};
#endif

#endif
}

namespace std::experimental
{
	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCFuture<t_CReturnType>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
#if DMibEnableSafeCheck > 0
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#endif
			>
		;
	};

	template <typename t_CReturnType, NMib::NConcurrency::EFutureOption t_Options, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCFutureWithOptions<t_CReturnType, t_Options>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
#if DMibEnableSafeCheck > 0
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#endif
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCFutureWithOptions<t_CReturnType, NMib::NConcurrency::EFutureOption_CaptureExceptions>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, (NMib::NConcurrency::EFutureCoroutineContextFlag)
				(
#if DMibEnableSafeCheck > 0
					NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#else
					NMib::NConcurrency::EFutureCoroutineContextFlag_None
#endif
					| NMib::NConcurrency::EFutureCoroutineContextFlag_CaptureExceptions
				)
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCFutureWithOptions<t_CReturnType, NMib::NConcurrency::EFutureOption_AllowReferences>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::EFutureCoroutineContextFlag_None
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCFutureWithOptions<t_CReturnType, NMib::NConcurrency::EFutureOption_AllowReferencesCaptureExceptions>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::EFutureCoroutineContextFlag_CaptureExceptions
			>
		;
	};
}
