// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always CContinuationCoroutineContext::CContinuationCoroutineContext(EContinuationCoroutineContextFlag _Flags) noexcept
		: m_Flags(_Flags)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;
	}

	inline_always CContinuationCoroutineContext::~CContinuationCoroutineContext() noexcept
	{
		if (m_pPreviousCoroutineHandler != this)
		{
			auto &ThreadLocal = **g_SystemThreadLocal;
			DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);
			ThreadLocal.m_pCurrentCoroutineHandler = m_pPreviousCoroutineHandler;
		}
	}

	CSuspendNever CContinuationCoroutineContext::initial_suspend()
#if DMibEnableSafeCheck <= 0
	 	noexcept
#endif
	{
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = **g_SystemThreadLocal;
		
		if (m_Flags & EContinuationCoroutineContextFlag_UnsafeReferenceParameters)
			fp_CheckUnsafeReferenceParams();
		if (m_Flags & EContinuationCoroutineContextFlag_UnsafeThisPointer)
			fp_CheckUnsafeThisPointer();

		ThreadLocal.m_bExpectCoroutineCall = false;
#endif

		DMibFastCheck(ThreadLocal.m_pCurrentCoroutineHandler == this);

		return {};
	}

	CSuspendAlways CContinuationCoroutineContext::final_suspend()
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
	TCContinuationCoroutineContextValue<t_CReturnType>::~TCContinuationCoroutineContextValue() noexcept
	{
		m_pContinuationData->m_Coroutine = nullptr;
	}

	inline_always TCContinuationCoroutineContextValue<void>::~TCContinuationCoroutineContextValue() noexcept
	{
		m_pContinuationData->m_Coroutine = nullptr;
	}

	template <typename t_CReturnType>
	void TCContinuationCoroutineContextValue<t_CReturnType>::return_value(t_CReturnType &&_Value)
	{
		m_pContinuationData->f_SetResult(fg_Move(_Value));
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCContinuationCoroutineContextValue<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReference<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			m_pContinuationData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value || NTraits::TCIsSame<CReturnNoReference, CExceptionPointer>::mc_Value)
			m_pContinuationData->f_SetException(fg_Forward<tf_CReturnType>(_Value));
		else
			m_pContinuationData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename tf_CReturnType>
	void TCContinuationCoroutineContextValue<void>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReference<tf_CReturnType>::CType;
		static_assert
			(
			 	NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value
			 	|| NException::TCIsExcption<CReturnNoReference>::mc_Value
			 	|| NTraits::TCIsSame<CReturnNoReference, CExceptionPointer>::mc_Value
			 	, "You can only return exceptions or async results"
			)
		;

		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value)
			m_pContinuationData->f_SetResult(fg_Forward<tf_CReturnType>(_Value));
		else
			m_pContinuationData->f_SetException(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename t_CReturnType>
	TCContinuation<t_CReturnType> TCContinuationCoroutineContext<t_CReturnType>::get_return_object()
	{
		TCContinuation<t_CReturnType> Continuation;

		auto *pContinuationData = Continuation.m_pData.f_Get();
		pContinuationData->m_Coroutine = TCCoroutineHandle<TCContinuationCoroutineContext>::from_promise(*this);

		this->m_pContinuationData = pContinuationData;

		return Continuation;
	}

	template <typename t_CReturnType>
	void TCContinuationCoroutineContext<t_CReturnType>::unhandled_exception()
	{
		if (this->m_Flags & EContinuationCoroutineContextFlag_CaptureExceptions)
		{
			try
			{
				throw;
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pContinuationData->f_SetException(_WrappedException.f_GetSpecific().m_pException);
			}
			catch (...)
			{
				this->m_pContinuationData->f_SetCurrentException();
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
				this->m_pContinuationData->f_SetException(_WrappedException.f_GetSpecific().m_pException);
			}
			// All other exceptionss falls through and crashes the application
		}
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<TCContinuationData<t_CReturnType>> TCContinuationCoroutineContext<t_CReturnType>::f_Suspend()
	{
		CContinuationCoroutineContext::f_Suspend();
		return fg_Explicit(this->m_pContinuationData);
	}
}

namespace NMib::NConcurrency
{
	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCContinuationAwaiter
	{
		TCContinuationAwaiter(TCContinuation<t_CReturnType> const &_Continuation, t_FExceptionTransform &&_fExceptionTransform)
			: mp_Continuation(_Continuation)
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

			mp_Continuation.f_OnResultSet
				(
					[this, CurrentActor, pKeepAlive = fg_Move(pKeepAlive), _Handle](TCAsyncResult<t_CReturnType> &&_Result) mutable
					{
						mp_Result = fg_Move(_Result);

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
			if constexpr (t_bUnwrap)
				return fg_UnwrapCoroutineAsyncResult(fg_Move(mp_Result), mp_fExceptionTransform);
			else
			{
				if constexpr (NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
					return fg_Move(mp_Result);
				else
				{
					if (mp_Result)
						return fg_Move(mp_Result);
					else
					{
						TCAsyncResult<t_CReturnType> Result;
						Result.f_SetException(mp_fExceptionTransform(fg_Move(mp_Result.f_GetException())));
						return Result;
					}
				}
			}
		}

	private:
		TCContinuation<t_CReturnType> mp_Continuation;
		TCAsyncResult<t_CReturnType> mp_Result;
		/*[[no_unique_address]]*/ t_FExceptionTransform mp_fExceptionTransform;
	};

	template <typename t_CReturnValue>
	auto TCContinuation<t_CReturnValue>::f_Wrap() const -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuation<t_CReturnValue>::operator co_await() const
	{
		return TCContinuationAwaiter<t_CReturnValue, true>(*this, nullptr);
	}

	template <typename t_CReturnValue>
	auto TCContinuation<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false>(*m_pWrapped, nullptr);
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithError<t_CReturnValue>::f_Wrap() -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithError<t_CReturnValue>::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_Continuation
			 	, this->fp_GetTransformer()
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCContinuationWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await()
	{
		return TCContinuationAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	m_pWrapped->m_Continuation
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
		static constexpr NMib::NConcurrency::EContinuationCoroutineContextFlag mc_Value = NMib::NConcurrency::EContinuationCoroutineContextFlag_UnsafeThisPointer; // Assume lambda
	};

	template <typename tfp_CThis>
	struct TCCoroutineContextFlagsFromParams_This<tfp_CThis, true>
	{
		static constexpr NMib::NConcurrency::EContinuationCoroutineContextFlag mc_Value
			= NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActor>::mc_Value
			? NMib::NConcurrency::EContinuationCoroutineContextFlag_None
			: NMib::NConcurrency::EContinuationCoroutineContextFlag_UnsafeThisPointer
		;
	};

	template <typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams
	{
		static constexpr NMib::NConcurrency::EContinuationCoroutineContextFlag mc_Value = NMib::NConcurrency::EContinuationCoroutineContextFlag_None;
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
		static constexpr NMib::NConcurrency::EContinuationCoroutineContextFlag mc_Value =
			(
				fg_AnyIsReference<tp_CParams...>()
				? NMib::NConcurrency::EContinuationCoroutineContextFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::EContinuationCoroutineContextFlag_None
			)
			| TCCoroutineContextFlagsFromParams_This<tp_CFirstParam>::mc_Value;
		;
	};
#else
	template <typename tp_CFirstParam, typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams<tp_CFirstParam, tp_CParams...>
	{
		static constexpr NMib::NConcurrency::EContinuationCoroutineContextFlag mc_Value =
			(
				(... || NMib::NTraits::TCIsReference<tp_CParams>::mc_Value)
				? NMib::NConcurrency::EContinuationCoroutineContextFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::EContinuationCoroutineContextFlag_None
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
	struct coroutine_traits<NMib::NConcurrency::TCContinuation<t_CReturnType>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCContinuationCoroutineContextWithFlags
			<
				t_CReturnType
#if DMibEnableSafeCheck > 0
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#endif
			>
		;
	};

	template <typename t_CReturnType, NMib::NConcurrency::EContinuationOption t_Options, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCContinuationWithOptions<t_CReturnType, t_Options>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCContinuationCoroutineContextWithFlags
			<
				t_CReturnType
#if DMibEnableSafeCheck > 0
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#endif
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCContinuationWithOptions<t_CReturnType, NMib::NConcurrency::EContinuationOption_CaptureExceptions>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCContinuationCoroutineContextWithFlags
			<
				t_CReturnType
				, (NMib::NConcurrency::EContinuationCoroutineContextFlag)
				(
#if DMibEnableSafeCheck > 0
					NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
#else
					NMib::NConcurrency::EContinuationCoroutineContextFlag_None
#endif
					| NMib::NConcurrency::EContinuationCoroutineContextFlag_CaptureExceptions
				)
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCContinuationWithOptions<t_CReturnType, NMib::NConcurrency::EContinuationOption_AllowReferences>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCContinuationCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::EContinuationCoroutineContextFlag_None
			>
		;
	};

	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCContinuationWithOptions<t_CReturnType, NMib::NConcurrency::EContinuationOption_AllowReferencesCaptureExceptions>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCContinuationCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::EContinuationCoroutineContextFlag_CaptureExceptions
			>
		;
	};
}
