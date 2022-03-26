// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always CFutureCoroutineContext::CFutureCoroutineContext(ECoroutineFlag _Flags) noexcept
		: m_Flags(_Flags)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		m_pPreviousCoroutineHandler = ThreadLocal.m_pCurrentCoroutineHandler;
		ThreadLocal.m_pCurrentCoroutineHandler = this;
		m_Flags |= ThreadLocal.m_ExtraCoroutineFlags;
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

	inline_always auto CFutureCoroutineContext::initial_suspend() noexcept -> CInitialSuspend
	{
		return {};
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnType>
	TCFutureCoroutineContextShared<t_CReturnType>::~TCFutureCoroutineContextShared() noexcept
	{
		if (m_pPromiseData)
		{
			auto &PromiseData = *m_pPromiseData;
			if (PromiseData.m_bPendingResult)
				PromiseData.f_OnResult();

			if (PromiseData.m_Coroutine)
				PromiseData.m_Coroutine = nullptr;
		}
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnType>
	void TCFutureCoroutineContextShared<t_CReturnType>::f_SetOwner(TCWeakActor<> const &_CoroutineOwner)
	{
		m_pPromiseData->m_CoroutineOwner = _CoroutineOwner;
	}
#endif

	template <typename t_CReturnType>
	void TCFutureCoroutineContext<t_CReturnType>::return_value(t_CReturnType &&_Value)
	{
		this->m_pPromiseData->f_SetResultNoReport(fg_Move(_Value));
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCFutureCoroutineContext<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			this->m_pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value)
		{
			static_assert
				(
				 	(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
				 	&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
				 	, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			try
			{
				std::rethrow_exception(_Value);
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
			}
			catch (...)
			{
				this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
			}
		}
		else
			this->m_pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename tf_CReturnType>
	void TCFutureCoroutineContext<void>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		static_assert
			(
			 	NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value
				|| NTraits::TCIsSame<CReturnNoReference, TCPromise<void>>::mc_Value
			 	|| NException::TCIsExcption<CReturnNoReference>::mc_Value
			 	|| NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value
			 	, "You can only return exceptions, async results or promises"
			)
		;

		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value)
			this->m_pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, TCPromise<void>>::mc_Value)
			this->m_pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else
		{
			if constexpr (NException::TCIsExcption<CReturnNoReference>::mc_Value)
			{
				static_assert
					(
						(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
						&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
						, "Only safe to newly created exceptions, otherwise use exception pointers"
					)
				;
				this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
			}
			else
			{
				NException::CDisableExceptionFilterScope DisableExceptionFilter;

				try
				{
					std::rethrow_exception(_Value);
				}
				catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
				{
					this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
				}
				catch (...)
				{
					this->m_pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
				}
			}
		}
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> TCFutureCoroutineContextShared<t_CReturnType>::fp_GetReturnObject()
	{
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = **g_SystemThreadLocal;
		bool bSafeCall = ThreadLocal.m_bExpectCoroutineCall;
		ThreadLocal.m_bExpectCoroutineCall = false;
#endif
		NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> pPromiseDataShared(fg_Construct());
#if DMibEnableSafeCheck > 0
		pPromiseDataShared->m_bFutureGotten = true;
#endif
		auto *pPromiseData = pPromiseDataShared.f_Get();
		pPromiseData->m_Coroutine = TCCoroutineHandle<TCFutureCoroutineContextShared>::from_promise(*this);
		pPromiseData->m_bIsCoroutine = true;
#if DMibEnableSafeCheck > 0
		pPromiseData->m_CoroutineOwner = fg_CurrentActor();
		this->fp_UpdateSafeCall(bSafeCall);
		if (bSafeCall)
		{
			DMibFastCheck(ThreadLocal.m_PromiseThreadLocal.m_pOnResultSetConsumedBy == pPromiseData || ThreadLocal.m_PromiseThreadLocal.m_pOnResultSetConsumedBy == this);
			ThreadLocal.m_PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = static_cast<CPromiseDataBase const *>(pPromiseData);
		}
#endif
		this->m_pPromiseData = pPromiseData;

		return pPromiseDataShared;
	}

	template <typename t_CReturnType>
	mark_no_coroutine_debug TCFuture<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::get_return_object()
	{
		return TCFuture<t_CReturnType>(fp_GetReturnObject());
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContextShared<t_CReturnType>::unhandled_exception()
	{
		if (this->m_Flags & ECoroutineFlag_CaptureExceptions)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			try
			{
				throw;
			}
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
			}
			catch (...)
			{
				this->m_pPromiseData->f_SetCurrentExceptionNoReport();
			}
		}
		else
		{
			try
			{
				throw;
			}
#if DMibEnableSafeCheck > 0
			catch (NException::CDebugException const &)
			{
				this->m_pPromiseData->f_SetCurrentExceptionNoReport();
			}
#endif
			catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
			{
				this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
			}
			// All other exceptionss falls through and crashes the application
		}
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::TCFutureCoroutineKeepAlive(TCActor<> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: mp_Actor(fg_Move(_Actor))
		, mp_pPromiseData(fg_Move(_pPromiseData))
	{
		DMibFastCheck(mp_pPromiseData);
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::TCFutureCoroutineKeepAlive(TCWeakActor<> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: mp_WeakActor(fg_Move(_Actor))
		, mp_pPromiseData(fg_Move(_pPromiseData))
	{
		DMibFastCheck(mp_pPromiseData);
	}

	template <typename t_CReturnType>
	TCActor<CActor> TCFutureCoroutineKeepAlive<t_CReturnType>::f_MoveActor()
	{
		if (mp_Actor)
			return fg_Move(mp_Actor);
		return mp_WeakActor.f_Lock();
	}

	template <typename t_CReturnType>
	bool TCFutureCoroutineKeepAlive<t_CReturnType>::f_HasValidCoroutine() const
	{
		DMibFastCheck(mp_pPromiseData);
		return !!mp_pPromiseData->m_Coroutine;
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::TCFutureCoroutineKeepAliveImplicit(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: mp_pPromiseData(fg_Move(_pPromiseData))
	{
		DMibFastCheck(mp_pPromiseData);
	}

	template <typename t_CReturnType>
	bool TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::f_HasValidCoroutine() const
	{
		DMibFastCheck(mp_pPromiseData);
		return !!mp_pPromiseData->m_Coroutine;
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::f_KeepAlive(TCActor<> &&_Actor)
	{
		DMibFastCheck(_Actor);
		if (this->m_Flags & ECoroutineFlag_BreakSelfReference)
			return TCFutureCoroutineKeepAlive<t_CReturnType>(_Actor.f_Weak(), fg_Explicit(this->m_pPromiseData));
		else
			return TCFutureCoroutineKeepAlive<t_CReturnType>(fg_Move(_Actor), fg_Explicit(this->m_pPromiseData));
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::f_KeepAliveImplicit()
	{
		return TCFutureCoroutineKeepAliveImplicit<t_CReturnType>(fg_Explicit(this->m_pPromiseData));
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContextShared<t_CReturnType>::f_Abort()
	{
		auto pPromiseData = this->m_pPromiseData;
		if (!pPromiseData || !pPromiseData->m_Coroutine)
			return;
		auto Coroutine = fg_Exchange(pPromiseData->m_Coroutine, nullptr);
		Coroutine.destroy();
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContextShared<t_CReturnType>::f_ResumeException()
	{
		NException::CDisableExceptionFilterScope DisableExceptionFilter;

		try
		{
			throw;
		}
		catch (CExceptionCoroutineWrapper const &_WrappedException) // When a co_await returns an exception
		{
			this->m_pPromiseData->f_SetExceptionNoReport(_WrappedException.f_GetSpecific().m_pException);
		}
		catch (...)
		{
			this->m_pPromiseData->f_SetCurrentExceptionNoReport();
		}
	}
}

namespace NMib::NConcurrency
{
	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCFutureAwaiter
	{
		TCFutureAwaiter(TCFuture<t_CReturnType> &&_Future, t_FExceptionTransform &&_fExceptionTransform)
			: mp_Future(fg_Move(_Future))
			, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
		{
		}

		bool await_ready() noexcept
		{
			if (mp_Future.f_ObserveIfAvailable())
			{
				auto Future = fg_Move(mp_Future);
				mp_Result = Future.f_MoveResult();
				return true;
			}
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			DMibFastCheck(fg_CurrentActor());
			DMibFastCheck(fg_CurrentActorProcessingOrOverridden());

			auto &CoroutineContext = _Handle.promise();

			auto Future = fg_Move(mp_Future);
			if
				(
					Future.f_OnResultSetOrAvailable
					(
						[this, _Handle, KeepAlive = CoroutineContext.f_KeepAlive(fg_CurrentActor())](TCAsyncResult<t_CReturnType> &&_Result) mutable
						{
							auto Actor = KeepAlive.f_MoveActor();
							if (!Actor)
								return;

							auto pRealActor = Actor.f_GetRealActor();
							pRealActor->f_QueueProcess
								(
									[this, Actor, _Handle, KeepAlive = fg_Move(KeepAlive), Result = fg_Move(_Result)]() mutable
									{
#if DMibEnableSafeCheck > 0
										if (!KeepAlive.f_HasValidCoroutine())
											return; // Can happen when f_Suspend throws
#endif

										auto &CoroutineContext = _Handle.promise();
										mp_Result = fg_Move(Result);
										CCurrentActorScope CurrentActorScope(Actor);
										bool bAborted = false;
										auto RestoreStates = CoroutineContext.f_Resume(bAborted);
										if (!bAborted)
											_Handle.resume();
									}
								)
							;
						}
					)
				)
			{
				mp_Result = Future.f_MoveResult();
				return false;
			}

			CoroutineContext.f_Suspend(true);
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
		TCFuture<t_CReturnType> mp_Future;
		TCAsyncResult<t_CReturnType> mp_Result;
		[[no_unique_address]] t_FExceptionTransform mp_fExceptionTransform;
	};

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_HasData(void const *_pData) const
	{
		return _pData == (void const *)static_cast<NPrivate::CPromiseDataBase const *>(mp_pData.f_Get());
	}
#endif

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true>(fg_Move(*this), nullptr);
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false>(fg_Move(*m_pWrapped), nullptr);
	}

	template <typename t_CReturnValue>
	TCFutureWithError<t_CReturnValue>::TCFutureWithError(TCFuture<t_CReturnValue> &&_Future, NStr::CStr const &_Error)
		: CActorWithErrorBase{_Error}
		, m_Future(fg_Move(_Future))
	{
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	fg_Move(m_Future)
			 	, this->fp_GetTransformer()
			)
		;
	}

	template <typename t_CReturnValue>
	auto TCFutureWithError<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>>
			(
			 	fg_Move(m_pWrapped->m_Future)
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
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag_UnsafeThisPointer; // Assume lambda
	};

	template <typename tfp_CThis>
	struct TCCoroutineContextFlagsFromParams_This<tfp_CThis, true>
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value =
			NTraits::TCIsReference<tfp_CThis>::mc_Value
			?
			(
				(
					NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActor>::mc_Value
					|| NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActorInternal>::mc_Value
					|| NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CAllowUnsafeThis>::mc_Value
				)
				? NMib::NConcurrency::ECoroutineFlag_None
				: NMib::NConcurrency::ECoroutineFlag_UnsafeThisPointer
			)
			: NMib::NConcurrency::ECoroutineFlag_None
		;
	};

	template <typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag_None;
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
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value =
			(
				fg_AnyIsReference<tp_CParams...>()
				? NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::ECoroutineFlag_None
			)
			| TCCoroutineContextFlagsFromParams_This
			<
				typename TCChooseType
				<
					NTraits::TCIsPointer<tp_CFirstParam>::mc_Value
					, typename NTraits::TCAddLValueReference<typename NTraits::TCRemovePointer<tp_CFirstParam>::CType>::CType
					, tp_CFirstParam
				>
				::CType
			>
			::mc_Value
		;
	};
#else
	template <typename tp_CFirstParam, typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams<tp_CFirstParam, tp_CParams...>
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value =
			(
				(... || NMib::NTraits::TCIsReference<tp_CParams>::mc_Value)
				? NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
				: NMib::NConcurrency::ECoroutineFlag_None
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
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContext<t_CReturnType>;
#endif
	};
}
