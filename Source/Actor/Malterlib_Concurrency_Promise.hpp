// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture() = default;

	template <typename t_CReturnValue>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> TCFuture<t_CReturnValue>::fsp_CreateData()
	{
		return fg_Construct(CPromiseConstructDirectFuture());
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NException::CExceptionPointer &&_pException)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetException(fg_Move(_pException));
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NException::CExceptionBase const &_Exception)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetException(_Exception.f_ExceptionPointer());
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(CInitializationValue &&_ReturnValue)
		requires(!NTraits::cIsSame<CInitializationValue, NException::CExceptionPointer>)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		if constexpr (NTraits::cIsVoid<t_CReturnValue>)
			pData->m_Result.f_SetResult();
		else
			pData->m_Result.f_SetResult(fg_Move(_ReturnValue));
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(CInitializationValue const &_ReturnValue)
		requires(!NTraits::cIsSame<CInitializationValue, NException::CExceptionPointer>)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		if constexpr (NTraits::cIsVoid<t_CReturnValue>)
			pData->m_Result.f_SetResult();
		else
			pData->m_Result.f_SetResult(_ReturnValue);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCAsyncResult<CValue> &&_ReturnValue)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetResult(fg_Move(_ReturnValue));
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCAsyncResult<CValue> const &_ReturnValue)
		: mp_pData(fsp_CreateData())
	{
		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetResult(_ReturnValue);
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_ForceDelete()
	{
#if DMibEnableSafeCheck > 0
		mp_pData->m_RefCount.f_FutureToPromise();
#endif
		mp_pData.f_ForceDelete();
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_Clear()
	{
		auto pData = mp_pData.f_Detach();
		if (pData && pData->m_RefCount.f_DecreaseFuture())
			fg_DeleteObject(NMemory::CDefaultAllocator(), pData);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::~TCFuture()
	{
		f_Clear();
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData)
		: mp_pData(fg_Move(_pData))
	{
		DMibFastCheck(!mp_pData || mp_pData->m_bFutureGotten);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData, NPrivate::CFutureConstructIgnoreFutureGotten)
		: mp_pData(fg_Move(_pData))
	{
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData, NPrivate::CFutureConstructSetFutureGotten)
		: mp_pData(fg_Move(_pData))
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(!mp_pData->m_bFutureGotten);
		mp_pData->m_bFutureGotten = true;
#endif
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &TCFuture<t_CReturnValue>::operator = (TCFuture &&_Other)
	{
		mp_pData = fg_Move(_Other.mp_pData);
		return *this;
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator = (CInitializationValue &&_ReturnValue) -> TCFuture &
	{
		DMibFastCheck(!mp_pData);
		mp_pData = fsp_CreateData();

		auto *pData = mp_pData.f_Get();
		if constexpr (NTraits::cIsVoid<t_CReturnValue>)
			pData->m_Result.f_SetResult();
		else
			pData->m_Result.f_SetResult(fg_Move(_ReturnValue));

		return *this;
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator = (CInitializationValue const &_ReturnValue) -> TCFuture &
	{
		DMibFastCheck(!mp_pData);
		mp_pData = fsp_CreateData();

		auto *pData = mp_pData.f_Get();
		if constexpr (NTraits::cIsVoid<t_CReturnValue>)
			pData->m_Result.f_SetResult();
		else
			pData->m_Result.f_SetResult(_ReturnValue);

		return *this;
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator = (TCAsyncResult<CValue> &&_ReturnValue) -> TCFuture &
	{
		DMibFastCheck(!mp_pData);
		mp_pData = fsp_CreateData();

		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetResult(fg_Move(_ReturnValue));

		return *this;
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator = (TCAsyncResult<CValue> const &_ReturnValue) -> TCFuture &
	{
		DMibFastCheck(!mp_pData);
		mp_pData = fsp_CreateData();

		auto *pData = mp_pData.f_Get();
		pData->m_Result.f_SetResult(_ReturnValue);

		return *this;
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCFuture &&_Other)
		: mp_pData(fg_Move(_Other.mp_pData))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &_pData)
		: mp_pData(_pData)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCPromise const &_Other)
		: mp_pData(_Other.mp_pData)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCPromise &&_Other)
		: mp_pData(fg_Move(_Other.mp_pData))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &TCPromise<t_CReturnValue>::operator = (TCPromise const &_Other)
	{
		mp_pData = _Other.mp_pData;

		return *this;
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &TCPromise<t_CReturnValue>::operator = (TCPromise &&_Other)
	{
		mp_pData = fg_Move(_Other.mp_pData);

		return *this;
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CConcurrencyThreadLocal &_ThreadLocal)
		: mp_pData(fg_Construct(_ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal, 0))
	{
		DMibFastCheck(!_ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal.m_bSupendOnInitialSuspend); // It's not allowed to have a actor call that isn't a co-routine.
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructConsume, CConcurrencyThreadLocal &_ThreadLocal)
		: mp_pData
		(
			NPrivate::fg_ConsumePromise<t_CReturnValue, NPrivate::EConsumePromiseFlag_UsePromise>
			(
				_ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal
				, nullptr
				, 0
#if DMibEnableSafeCheck > 0
				, nullptr
				, NPrivate::EConsumePromiseDebugFlag_ResetSafeCall
#endif
			)
		)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructNoConsume, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal)
		: mp_pData(fg_Construct(_ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal, _RefCount))
	{
		DMibFastCheck(!_ThreadLocal.m_SystemThreadLocal.m_PromiseThreadLocal.m_bSupendOnInitialSuspend); // It's not allowed to have a actor call that isn't a co-routine.
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(FOnResult &&_fOnResult)
		: mp_pData(fg_Construct(fg_Move(_fOnResult), 0))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(FOnResult &&_fOnResult, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal)
		: mp_pData(fg_Construct(fg_Move(_fOnResult), _RefCount))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructDiscardResult)
		: mp_pData(fg_Construct(CPromiseConstructDiscardResult(), 0))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructDiscardResult, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal)
		: mp_pData(fg_Construct(CPromiseConstructDiscardResult(), _RefCount))
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructEmpty)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(CPromiseConstructEmpty, int32 _RefCount, CConcurrencyThreadLocal &_ThreadLocal)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::~TCPromise()
	{
		f_Clear();
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_ClearResultSet()
	{
		auto pData = mp_pData.f_Detach();
		DMibFastCheck(pData);
		DMibFastCheck(pData->m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & (EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult));

		if (pData->m_RefCount.f_PromiseDecrease())
			fg_DeleteObject(NMemory::CDefaultAllocator(), pData);
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_Clear()
	{
		auto pData = mp_pData.f_Detach();
		if (!pData)
			return;

		if (pData->m_RefCount.f_IsLastPromiseReference())
			pData->f_ReportNothingSet();

		if (pData->m_RefCount.f_PromiseDecrease())
			fg_DeleteObject(NMemory::CDefaultAllocator(), pData);
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_IsValid() const
	{
		return !!mp_pData;
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_Future() const
	{
		auto pData = mp_pData.f_Get();
#if DMibEnableSafeCheck > 0
		DMibFastCheck(pData);
		DMibFastCheck(!pData->m_bFutureGotten); // A promise can only be used for one future
		DMibFastCheck(!pData->m_BeforeSuspend.m_bPromiseConsumed); // Use f_FutureConsumed()
		pData->m_bFutureGotten = true;
#endif
		pData->m_RefCount.f_IncreaseFuture();
		return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_MoveFuture()
	{
		auto pData = mp_pData.f_Detach();
#if DMibEnableSafeCheck > 0
		DMibFastCheck(pData);
		DMibFastCheck(!pData->m_bFutureGotten); // A promise can only be used for one future
		DMibFastCheck(!pData->m_BeforeSuspend.m_bPromiseConsumed); // Use f_MoveFutureConsumed()
		pData->m_bFutureGotten = true;
#endif

		if (pData->m_RefCount.f_PromiseToFuture())
			pData->f_ReportNothingSet();

		return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::fp_AttachFutureIgnoreFutureGotten()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData);

#if DMibEnableSafeCheck > 0
		TCFuture<t_CReturnValue> Return{NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)}, NPrivate::CFutureConstructIgnoreFutureGotten{}};
		return Return;
#else
		return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
#endif
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_FutureConsumed() const
	{
		auto pData = mp_pData.f_Get();
#if DMibEnableSafeCheck > 0
		DMibFastCheck(pData);
		DMibFastCheck(!pData->m_bFutureGotten); // A promise can only be used for one future
		pData->m_bFutureGotten = true;
#endif
		if (pData->m_BeforeSuspend.m_bPromiseConsumed)
			return {};
		else
		{
			pData->m_RefCount.f_IncreaseFuture();
			return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
		}
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_MoveFutureConsumed()
	{
		if (mp_pData->m_BeforeSuspend.m_bPromiseConsumed)
		{
			f_Clear();
			return {};
		}

		auto pData = mp_pData.f_Detach();
#if DMibEnableSafeCheck > 0
		DMibFastCheck(pData);
		DMibFastCheck(!pData->m_bFutureGotten); // A promise can only be used for one future
		pData->m_bFutureGotten = true;
#endif
		if (pData->m_RefCount.f_PromiseToFuture())
			pData->f_ReportNothingSet();

		return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_MoveFutureConsumedResultSet()
	{
		if (mp_pData->m_BeforeSuspend.m_bPromiseConsumed)
		{
			f_ClearResultSet();
			return {};
		}

		auto pData = mp_pData.f_Detach();
#if DMibEnableSafeCheck > 0
		DMibFastCheck(pData);
		DMibFastCheck(!pData->m_bFutureGotten); // A promise can only be used for one future
		pData->m_bFutureGotten = true;
#endif
		[[maybe_unused]] bool bPromiseDone = pData->m_RefCount.f_PromiseToFuture();
		DMibCheck(bPromiseDone);

		return NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>>{fg_Attach(pData)};
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_IsSet() const
	{
		return this->mp_pData->f_IsSet();
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_IsDiscarded() const
	{
		return this->mp_pData->f_IsDiscarded();
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_SetResult() const
	{
		this->mp_pData->f_SetResult();
	}
	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetResult(tf_CResult &&_Result) const
	{
		this->mp_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
	}
	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetException(tf_CResult &&_Result) const
	{
		this->mp_pData->f_SetException(fg_Forward<tf_CResult>(_Result));
	}

	template <typename t_CReturnValue>
	template <typename tf_CException, typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const
	{
		if (!_Exception)
			this->mp_pData->f_SetException(fg_Forward<tf_CException>(_Exception));
		else
			this->mp_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_SetCurrentException() const
	{
		this->mp_pData->f_SetCurrentException();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (CVoidTag const &)
	{
		this->mp_pData->f_SetResult();
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult, TCEnableIf<!NTraits::cIsBaseOf<NTraits::TCRemoveReference<tf_CResult>, NException::CExceptionBase>> *>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (tf_CResult &&_Result)
	{
		this->mp_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	template <typename tf_CType, TCEnableIf<NTraits::cIsBaseOf<NTraits::TCRemoveReference<tf_CType>, NException::CExceptionBase>> *>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (tf_CType &&_Exception)
	{
		this->mp_pData->f_SetException(fg_Forward<tf_CType>(_Exception));
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (NException::CExceptionPointer &&_pException)
	{
		this->mp_pData->f_SetException(fg_Move(_pException));
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (NException::CExceptionPointer const &_pException)
	{
		this->mp_pData->f_SetException(_pException);
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (NException::CExceptionPointer &_pException)
	{
		this->mp_pData->f_SetException(_pException);
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (TCFuture<t_CReturnValue> &&_Future)
	{
		fg_Move(_Future) > fg_TempCopy(*this);
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<=
		(
			TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_BoundActorCall
		)
	{
		fg_Move(_BoundActorCall) > fg_TempCopy(*this);
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	auto TCPromise<t_CReturnValue>::f_ReceiveAny() const & -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise>
	{
		return NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<t_CReturnValue>>{*this};
	}

	template <typename t_CReturnValue>
	auto TCPromise<t_CReturnValue>::f_ReceiveAny() && -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise>
	{
		return NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<t_CReturnValue>>{fg_Move(*this)};
	}

	template <>
	auto TCPromise<void>::f_ReceiveAny() const & -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>;

	template <>
	auto TCPromise<void>::f_ReceiveAnyUnwrap() const & -> NPrivate::CPromiseReceiveAnyUnwrapFunctor;

	template <>
	auto TCPromise<void>::f_ReceiveAny() && -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>;

	template <>
	auto TCPromise<void>::f_ReceiveAnyUnwrap() && -> NPrivate::CPromiseReceiveAnyUnwrapFunctor;

	namespace NPrivate
	{
		template <typename t_CTypeList>
		struct TCAllAsyncResultsAreVoidHelper
		{
		};

		template <typename ...tp_CParam>
		struct TCAllAsyncResultsAreVoidHelper<NMeta::TCTypeList<tp_CParam...>>
		{
			enum
			{
				mc_Value = NMeta::cAllIsTrue
				<
					NTraits::cIsVoid<tp_CParam>...
				>
			};
		};

		template <typename t_FFunctor>
		struct TCAllAsyncResultsAreVoid
		{
			enum
			{
				mc_Value = TCAllAsyncResultsAreVoidHelper
				<
					typename NTraits::TCMemberFunctionPointerTraits
					<
						decltype(&t_FFunctor::operator ())
					>::CParams
				>::mc_Value
			};
		};
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler>
	auto TCPromise<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const &
	{
		return [Promise = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				(
					[&]
					{
						if (!bFailed && !p_Results)
						{
							Promise.f_SetException(fg_Move(p_Results));
							bFailed = true;
						}
					}
					()
					, ...
				);

				if (bFailed)
					return;
				if constexpr (NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value)
					fResultHandler();
				else
					fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler>
	auto TCPromise<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) &&
	{
		return [Promise = fg_Move(*this), fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				(
					[&]
					{
						if (!bFailed && !p_Results)
						{
							Promise.f_SetException(fg_Move(p_Results));
							bFailed = true;
						}
					}
					()
					, ...
				);

				if (bFailed)
					return;
				if constexpr (NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value)
					fResultHandler();
				else
					fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	template <typename t_CReturnValue>
	auto TCPromise<t_CReturnValue>::operator % (NStr::CStr const &_ErrorString) const -> TCPromiseWithExceptionTransfomer<t_CReturnValue>
	{
		return TCPromiseWithExceptionTransfomer<t_CReturnValue>(*this, fg_ExceptionTransformer(nullptr, _ErrorString));
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator % (NStr::CStr const &_ErrorString) && -> TCFutureWithExceptionTransformer<t_CReturnValue, TCFuture>
	{
		return TCFutureWithExceptionTransformer<t_CReturnValue, TCFuture<t_CReturnValue>>(fg_Move(*this), fg_ExceptionTransformer(nullptr, _ErrorString));
	}

	template <typename t_CReturnValue>
	TCPromiseWithExceptionTransfomer<t_CReturnValue>::TCPromiseWithExceptionTransfomer(TCPromise<t_CReturnValue> const &_Promise, FExceptionTransformer &&_fTransformer)
		: CActorWithErrorBase(fg_Move(_fTransformer))
		, m_Promise(_Promise)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler>
	auto TCPromiseWithExceptionTransfomer<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) &&
	{
		return [Promise = fg_Move(m_Promise), fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), fTransformer = fg_Move(*this).f_GetTransformer()]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				(
					[&]
					{
						if (!bFailed && !p_Results)
						{
							Promise.f_SetException(fTransformer(p_Results.f_GetException()));
							bFailed = true;
						}
					}
					()
					, ...
				);

				if (bFailed)
					return;
				if constexpr (NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value)
					fResultHandler();
				else
					fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_OnResultSetOrAvailable(FOnResult &&_fOnResult)
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_BeforeSuspend.m_bNeedAtomicRead || !pData->m_BeforeSuspend.m_bOnResultSetAtInit);

		pData->m_fOnResult = fg_Move(_fOnResult);

		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet, NAtomic::gc_MemoryOrder_AcquireRelease);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_ResultFunctorSet)); // You can only observe the result once

#if DMibConfig_Concurrency_DebugActorCallstacks
		pData->m_OnResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_DataSet)
		{
			pData->m_fOnResult.f_Clear();
			return true;
		}

		return false;
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_OnResultSet(FOnResult &&_fOnResult)
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_BeforeSuspend.m_bNeedAtomicRead || !pData->m_BeforeSuspend.m_bOnResultSetAtInit);

		pData->m_fOnResult = fg_Move(_fOnResult);

		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet, NAtomic::gc_MemoryOrder_AcquireRelease);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_ResultFunctorSet)); // You can only observe the result once

#if DMibConfig_Concurrency_DebugActorCallstacks
		pData->m_OnResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_DataSet)
		{
			pData->m_fOnResult(fg_Move(pData->m_Result));
			pData->m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_OnResultSetNoClear(FOnResult &&_fOnResult)
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_BeforeSuspend.m_bNeedAtomicRead || !pData->m_BeforeSuspend.m_bOnResultSetAtInit);

		pData->m_fOnResult = fg_Move(_fOnResult);

		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet, NAtomic::gc_MemoryOrder_AcquireRelease);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_ResultFunctorSet)); // You can only observe the result once

#if DMibConfig_Concurrency_DebugActorCallstacks
		pData->m_OnResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_DataSet)
		{
			pData->m_fOnResult(fg_Move(pData->m_Result));
			return true;
		}

		return false;
	}

#if DMibConfig_Concurrency_DebugActorCallstacks
	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_Debug_SetCallstacks(CAsyncCallstacks &&_Callstacks)
	{
		mp_pData->m_Result.m_Callstacks = fg_Move(_Callstacks);
	}
#endif

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_DiscardResult()
	{
		auto pData = mp_pData.f_Get();
		pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_DiscardResult, NAtomic::gc_MemoryOrder_Relaxed);
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_ObserveIfAvailable()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_BeforeSuspend.m_bNeedAtomicRead);

		uint8 Expected = EFutureResultFlag_DataSet;

		auto constexpr c_FlagsToAdd = EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult;

		return pData->m_OnResultSet.f_CompareExchangeStrong(Expected, c_FlagsToAdd, NAtomic::gc_MemoryOrder_Acquire, NAtomic::gc_MemoryOrder_Relaxed);
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_ObserveIfAvailableRelaxed()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(!pData->m_BeforeSuspend.m_bNeedAtomicRead);

		uint8 Expected = EFutureResultFlag_DataSet;

		auto constexpr c_FlagsToAdd = EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult;

#ifdef DMibConcurrencyNonAtomicOnResultSet
		auto &OnResultSet = pData->m_OnResultSet.f_NonAtomic();
		if (OnResultSet == Expected)
		{
			OnResultSet = c_FlagsToAdd;
			return true;
		}

		return false;
#else
		return pData->m_OnResultSet.f_CompareExchangeStrong(Expected, c_FlagsToAdd, NAtomic::gc_MemoryOrder_Relaxed, NAtomic::gc_MemoryOrder_Relaxed);
#endif
	}

	template <typename t_CReturnValue>
	TCAsyncResult<t_CReturnValue> &&TCFuture<t_CReturnValue>::f_MoveResult()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet);
		return fg_Move(pData->m_Result);
	}

	template <typename t_CReturnValue>
	TCAsyncResult<t_CReturnValue> &&TCPromise<t_CReturnValue>::f_MoveResult()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet);
		return fg_Move(pData->m_Result);
	}

	template <typename t_CReturnValue>
	template <typename tf_FOnEmpty>
	void TCPromise<t_CReturnValue>::f_Abandon(tf_FOnEmpty &&_fOnEmpty)
	{
		auto pData = mp_pData.f_Get();
		if (!pData->m_Result.f_IsSet())
		{
			pData->m_Result.f_SetException(_fOnEmpty());
			EFutureResultFlag PreviousFlags;

#ifdef DMibConcurrencyNonAtomicOnResultSet
			if (pData->m_BeforeSuspend.m_bOnResultSetAtInit)
			{
				auto &OnResultSet = pData->m_OnResultSet.f_NonAtomic();
				PreviousFlags = (EFutureResultFlag)OnResultSet;
				OnResultSet |= EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet;
			}
			else
			{
				PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr
					(
						EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet
						, NAtomic::gc_MemoryOrder_AcquireRelease
					)
				;
			}
#else
			EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr
				(
					EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet
					, pData->f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease)
				)
			;
#endif

			DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once

#if DMibConfig_Concurrency_DebugActorCallstacks
			pData->m_ResultSetCallstack.f_Capture();
#endif
			if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
			{
				pData->m_fOnResult(fg_Move(pData->m_Result));
				pData->m_fOnResult.f_Clear();
			}
		}
		else
		{
			[[maybe_unused]] EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet, NAtomic::gc_MemoryOrder_Relaxed);

			DMibFastCheck(PreviousFlags & EFutureResultFlag_DataSet); // Result should already have been set
		}
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_IsValid() const
	{
		return !!mp_pData;
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnValue>
	void const *TCPromise<t_CReturnValue>::f_Debug_GetData()
	{
		return mp_pData.f_Get();
	}
#endif

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &&TCFuture<t_CReturnValue>::f_Move()
	{
		return fg_Move(*this);
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &&TCPromise<t_CReturnValue>::f_Move()
	{
		return fg_Move(*this);
	}

	inline_always CCaptureExceptionsHelper::operator CCaptureExceptionsHelperWithTransformer const &()
	{
		return CCaptureExceptionsHelperWithTransformer::ms_EmptyTransformer;
	}

	template <typename tf_CTransformerParam>
	CCaptureExceptionsHelperWithTransformer operator % (CCaptureExceptionsHelperWithTransformer const &_Helper, tf_CTransformerParam &&_Param)
	{
		return CCaptureExceptionsHelperWithTransformer{fg_ExceptionTransformer(fg_TempCopy(_Helper.m_fTransformer), fg_Forward<tf_CTransformerParam>(_Param))};
	}

	template <typename tf_CTransformerParam>
	CCaptureExceptionsHelperWithTransformer operator % (CCaptureExceptionsHelperWithTransformer &&_Helper, tf_CTransformerParam &&_Param)
	{
		return CCaptureExceptionsHelperWithTransformer{fg_ExceptionTransformer(fg_Move(_Helper.m_fTransformer), fg_Forward<tf_CTransformerParam>(_Param))};
	}

	template <typename ...tfp_CExceptions>
	CCaptureExceptionsHelperWithTransformer CCaptureExceptionsHelper::f_Specific() const
	{
		return CCaptureExceptionsHelperWithTransformer
			{
				[](NException::CExceptionPointer &&_pException) -> NException::CExceptionPointer
				{
					auto bFound = NException::fg_VisitException<tfp_CExceptions...>
						(
							_pException
							, [](auto &&)
							{
							}
						)
					;

					if (bFound)
						return fg_Move(_pException);
					else
						return nullptr;
				}
			}
		;
	}

	template <typename tf_CCoroutineContext>
	bool CFutureCoroutineContextOnResumeScopeAwaiter::await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
	{
		DMibFastCheck(fg_CurrentActor());

		auto pResult = mp_fOnResume();
		if (pResult)
		{
			mp_pThis->f_HandleAwaitedResult(fg_ConcurrencyThreadLocal(), &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor, fg_Move(pResult));
			return true;
		}

		return false;
	}

	template <typename t_CType>
	TCOnResumeResult<t_CType>::TCOnResumeResult(TCAsyncResult<t_CType> &&_Result)
		: m_Result(fg_Move(_Result))
	{
	}

	template <typename t_CType>
	NException::CExceptionPointer TCOnResumeResult<t_CType>::f_TryGetException() &&
	{
		if (m_Result)
			return {};

		return fg_Move(m_Result).f_GetException();
	}

	template <typename t_CType>
	void TCOnResumeResult<t_CType>::f_SetException(NException::CExceptionPointer &&_pException)
	{
		m_Result.f_Clear();
		m_Result.f_SetException(fg_Move(_pException));
	}

	template <typename t_CType>
	void TCOnResumeResult<t_CType>::f_ApplyValue(NPrivate::CPromiseDataBase *_pPromiseData)
	{
		DMibFastCheck(_pPromiseData->m_pDebugTypeId == NPrivate::fg_PromiseDataTypeId<t_CType>());
		auto *pTyped = static_cast<NPrivate::TCPromiseData<t_CType> *>(_pPromiseData);
		pTyped->f_SetResultNoReport(fg_Move(*m_Result));
	}

	template <typename tf_FOnResume>
	auto fg_OnResume(tf_FOnResume &&_fOnResume)
	{
		using CReturnType = decltype(_fOnResume());

		if constexpr (NTraits::cIsSame<CReturnType, NException::CExceptionPointer>)
		{
			return CFutureCoroutineContextOnResumeScopeAwaiter
				(
					[fOnResume = fg_Move(_fOnResume)]() mutable -> NStorage::TCUniquePointer<ICOnResumeResult>
					{
						auto pException = fOnResume();
						if (!pException)
							return {};  // Continue coroutine

						return fg_Construct<COnResumeException>(fg_Move(pException));
					}
				)
			;
		}
		else
		{
			using CValueType = typename CReturnType::CValue;
			return TCFutureCoroutineContextOnResumeScopeAwaiter<CValueType>
				(
					[fOnResume = fg_Move(_fOnResume)]() mutable -> NStorage::TCUniquePointer<ICOnResumeResult>
					{
						auto Result = fOnResume();
						if (!Result.f_IsSet())
							return {};  // Continue coroutine

						NStorage::TCUniquePointer<TCOnResumeResult<CValueType>> pResult = fg_Construct(fg_Move(Result));
						return pResult;
					}
				)
			;
		}
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData
		(
			CPromiseThreadLocal &_ThreadLocal
			, int32 _RefCount
#if DMibEnableSafeCheck > 0
			, EConsumeFutureOnResultSetFlag _DebugFlags
#endif
		)
		: TCPromiseData
		(
			fg_ConsumeFutureOnResultSet<t_CReturnValue>
			(
				_ThreadLocal
#if DMibEnableSafeCheck > 0
				, static_cast<CPromiseDataBase const *>(this)
				, _DebugFlags
#endif
			)
			, _RefCount
		)
	{
		DMibFastCheck(&f_GetAsyncResult() == static_cast<CAsyncResult *>(&m_Result));
	}

	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData(TCPromiseDataInit<t_CReturnValue> &&_Init, int32 _RefCount)
		: CPromiseDataBase
		(
			_RefCount
			, _Init.m_Flags
			, _Init.m_bOnResultSetAtInit
			, fg_GetHighestBitSet(fg_MaxConstexpr(TCAlignOfWithVoid<t_CReturnValue>::mc_Value, sizeof(void *)) / sizeof(void *))
#if DMibConfig_Concurrency_DebugFutures
			, fg_GetTypeName<t_CReturnValue>()
#endif
#if DMibEnableSafeCheck > 0
			, fg_PromiseDataTypeId<t_CReturnValue>()
#endif
		)
		, m_fOnResult{_Init.m_pOnResult ? fg_Move(*_Init.m_pOnResult) : nullptr}
	{
	}

	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData(CPromiseConstructDiscardResult, int32 _RefCount)
		: CPromiseDataBase
		(
			_RefCount
			, EFutureResultFlag_DiscardResult
			, true
			, fg_GetHighestBitSet(fg_MaxConstexpr(TCAlignOfWithVoid<t_CReturnValue>::mc_Value, sizeof(void *)) / sizeof(void *))
#if DMibConfig_Concurrency_DebugFutures
			, fg_GetTypeName<t_CReturnValue>()
#endif
#if DMibEnableSafeCheck > 0
			, fg_PromiseDataTypeId<t_CReturnValue>()
#endif
		)
	{
#if DMibConfig_Concurrency_DebugUnobservedException
		m_Callstack.f_Capture();
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		if (m_fOnResult)
			m_OnResultSetCallstack.f_Capture();
#endif

		DMibFastCheck(&f_GetAsyncResult() == static_cast<CAsyncResult *>(&m_Result));
	}

	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData(CPromiseConstructDirectFuture)
		: CPromiseDataBase
		(
			CPromiseDataBaseRefCount::mc_FutureBit - 1
			, EFutureResultFlag_DataSet
			, false
			, fg_GetHighestBitSet(fg_MaxConstexpr(TCAlignOfWithVoid<t_CReturnValue>::mc_Value, sizeof(void *)) / sizeof(void *))
#if DMibConfig_Concurrency_DebugFutures
			, fg_GetTypeName<t_CReturnValue>()
#endif
#if DMibEnableSafeCheck > 0
			, fg_PromiseDataTypeId<t_CReturnValue>()
#endif
		)
	{
	}

	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData(FOnResult &&_fOnResult, int32 _RefCount)
		: CPromiseDataBase
		(
			_RefCount
			, EFutureResultFlag_ResultFunctorSet
			, true
			, fg_GetHighestBitSet(fg_MaxConstexpr(TCAlignOfWithVoid<t_CReturnValue>::mc_Value, sizeof(void *)) / sizeof(void *))
#if DMibConfig_Concurrency_DebugFutures
			, fg_GetTypeName<t_CReturnValue>()
#endif
#if DMibEnableSafeCheck > 0
			, fg_PromiseDataTypeId<t_CReturnValue>()
#endif
		)
		, m_fOnResult(fg_Move(_fOnResult))
 	{
		DMibFastCheck(!!m_fOnResult);
#if DMibConfig_Concurrency_DebugUnobservedException
		m_Callstack.f_Capture();
#endif
#if DMibConfig_Concurrency_DebugActorCallstacks
		if (m_fOnResult)
			m_OnResultSetCallstack.f_Capture();
#endif
		DMibFastCheck(&f_GetAsyncResult() == static_cast<CAsyncResult *>(&m_Result));
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_Reset(FOnResult &&_fOnResult)
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
		DMibFastCheck(!fg_SystemThreadLocal().m_PromiseThreadLocal.m_pOnResultSet);

		m_fOnResult = fg_Move(_fOnResult);

#if DMibConfig_Concurrency_DebugActorCallstacks
		m_OnResultSetCallstack = {};
		m_ResultSetCallstack = {};
		m_OnResultSetCallstack.f_Capture();
#endif
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
			m_BeforeSuspend.m_bOnResultSetAtInit = false;
		m_Result = TCAsyncResult<t_CReturnValue>();
		m_OnResultSet = EFutureResultFlag_ResultFunctorSet;
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_Reset()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
		DMibFastCheck(!fg_SystemThreadLocal().m_PromiseThreadLocal.m_pOnResultSet);

		m_fOnResult.f_Clear();

#if DMibConfig_Concurrency_DebugActorCallstacks
		m_OnResultSetCallstack = {};
		m_ResultSetCallstack = {};
		m_PendingResultSetCallstack = {};
#endif

		m_BeforeSuspend.m_bOnResultSetAtInit = false;
		m_Result = TCAsyncResult<t_CReturnValue>();
		m_OnResultSet = EFutureResultFlag_None;
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_ReportNothingSet()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);

#ifdef DMibConcurrencyNonAtomicOnResultSet
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
		{
			if (m_OnResultSet.f_NonAtomic() & (EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult))
				return;
		}
		else
		{
			if (m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & (EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult))
				return;
		}
#else
		if (m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & (EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult))
			return;
#endif

		m_Result.f_SetException(fg_TempCopy(CAsyncResult::fs_ResultWasNotSetException()));

#ifdef DMibConcurrencyNonAtomicOnResultSet
		EFutureResultFlag PreviousFlags;
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
		{
			auto &OnResultSet = m_OnResultSet.f_NonAtomic();
			PreviousFlags = (EFutureResultFlag)OnResultSet;
			OnResultSet |= EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult;
		}
		else
		{
			PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr
				(
					EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult
					, NAtomic::gc_MemoryOrder_AcquireRelease
				)
			;
		}
#else
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr
			(
				EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult
				, f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease)
			)
		;
#endif

#if DMibConfig_Concurrency_DebugActorCallstacks
		if (!(PreviousFlags & EFutureResultFlag_DataSet))
			m_ResultSetCallstack.f_Capture();
#endif
		if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet)) == EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_ReportNothingSetAtExit()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
#ifdef DMibConcurrencyNonAtomicOnResultSet
		EFutureResultFlag PreviousFlags;
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
		{
			auto &OnResultSet = m_OnResultSet.f_NonAtomic();
			PreviousFlags = (EFutureResultFlag)OnResultSet;
			OnResultSet |= EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet;
		}
		else
		{
			PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr
				(
					EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet
					, NAtomic::gc_MemoryOrder_Acquire
				)
			;
		}
#else
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr
			(
				EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet
				, f_MemoryOrder(NAtomic::gc_MemoryOrder_Acquire)
			)
		;
#endif

#if DMibConfig_Concurrency_DebugActorCallstacks
		if (!(PreviousFlags & EFutureResultFlag_DataSet))
			m_ResultSetCallstack.f_Capture();
#endif
		if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet)) == EFutureResultFlag_ResultFunctorSet)
		{
			if (!m_AfterSuspend.m_bPendingResult)
				m_Result.f_SetException(fg_TempCopy(CAsyncResult::fs_ResultWasNotSetException()));
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
		else if
			(
				((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet | EFutureResultFlag_DiscardResult)) == EFutureResultFlag_DataSet)
				&& !m_BeforeSuspend.m_bIsGenerator
			)
		{
			if (!m_Result)
			{
#if DMibConfig_Concurrency_DebugUnobservedException
				fg_ReportUnobservedException(m_Result.f_GetException(), "\n" + m_Callstack.f_GetString(4));
#else
				fg_ReportUnobservedException(m_Result.f_GetException());
#endif
			}
		}
	}

	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::~TCPromiseData()
	{
#ifdef DMibNeedDebugException
		auto &ThreadLocal = fg_SystemThreadLocal();

		if
			(
				ThreadLocal.m_PromiseThreadLocal.m_bCaptureDebugException
				&& !(m_OnResultSet.f_Load() & EFutureResultFlag_DataSet)
				&& NException::fg_UncaughtExceptions()
				&& ThreadLocal.m_PromiseThreadLocal.m_pOnResultSetConsumedBy == (void *)this
			)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			auto pCurrentException = NException::fg_CurrentException();

			if (pCurrentException && fg_IsDebugException(pCurrentException))
				f_SetException(fg_Move(pCurrentException));
		}
#endif
		if (m_AfterSuspend.m_bCoroutineValid)
		{
#if DMibEnableSafeCheck > 0

			DMibFastCheck
				(
					fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder == m_pCoroutineOwner
					|| m_BeforeSuspend.m_bIsGenerator
					//|| fg_ConcurrencyThreadLocal().m_pCurrentlyProcessingActorHolder->f_IsHolderDestroyed()
				)
			;
#endif
			NPrivate::fg_DestroyCoroutine(m_Coroutine);

			f_ReportNothingSetAtExit();
		}
		else if (!m_BeforeSuspend.m_bIsCoroutine)
			f_ReportNothingSetAtExit();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_OnResult()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
		EFutureResultFlag PreviousFlags;
#ifdef DMibConcurrencyNonAtomicOnResultSet
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
		{
			auto &OnResultSet = m_OnResultSet.f_NonAtomic();
			PreviousFlags = (EFutureResultFlag )OnResultSet;
			OnResultSet |= EFutureResultFlag_DataSet;
		}
		else
			PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease));
#else
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease));
#endif

		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once
		DMibFastCheck(m_Result.f_IsSet());

#if DMibConfig_Concurrency_DebugActorCallstacks
		if (!(PreviousFlags & EFutureResultFlag_DataSet))
			m_ResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_OnResultNoClear()
	{
		DMibFastCheck(!m_AfterSuspend.m_bPendingResult);
		EFutureResultFlag PreviousFlags;
#ifdef DMibConcurrencyNonAtomicOnResultSet
		if (m_BeforeSuspend.m_bOnResultSetAtInit)
		{
			auto &OnResultSet = m_OnResultSet.f_NonAtomic();
			PreviousFlags = (EFutureResultFlag )OnResultSet;
			OnResultSet |= EFutureResultFlag_DataSet;
		}
		else
			PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease));
#else
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, f_MemoryOrder(NAtomic::gc_MemoryOrder_AcquireRelease));
#endif

		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once
		DMibFastCheck(m_Result.f_IsSet());

#if DMibConfig_Concurrency_DebugActorCallstacks
		if (!(PreviousFlags & EFutureResultFlag_DataSet))
			m_ResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
			m_fOnResult(fg_Move(m_Result));
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_OnResultRelaxed()
	{
#ifdef DMibConcurrencyNonAtomicOnResultSet
		auto &OnResultSet = m_OnResultSet.f_NonAtomic();
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)OnResultSet;
		OnResultSet |= EFutureResultFlag_DataSet;
#else
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, NAtomic::gc_MemoryOrder_Relaxed);
#endif

		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once
		DMibFastCheck(m_Result.f_IsSet());
		DMibFastCheck
			(
				m_RefCount.f_Get() == 0
				|| m_RefCount.f_Get() == (NPrivate::CPromiseDataBaseRefCount::mc_FutureBit - 1)
			)
		;
#if DMibConfig_Concurrency_DebugActorCallstacks
		if (!(PreviousFlags & EFutureResultFlag_DataSet))
			m_ResultSetCallstack.f_Capture();
#endif
		if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	bool TCPromiseData<t_CReturnValue>::f_IsSet() const
	{
		return (m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DataSet) != EFutureResultFlag_None;
	}

	template <typename t_CReturnValue>
	bool TCPromiseData<t_CReturnValue>::f_IsDiscarded() const
	{
		return (m_OnResultSet.f_Load(NAtomic::gc_MemoryOrder_Relaxed) & EFutureResultFlag_DiscardResult) != EFutureResultFlag_None;
	}

	inline_always NAtomic::CMemoryOrder CPromiseDataBase::f_MemoryOrder(NAtomic::CMemoryOrder _Default) const
	{
		return m_BeforeSuspend.m_bOnResultSetAtInit ? NAtomic::gc_MemoryOrder_Relaxed : _Default;
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult()
	{
		m_Result.f_SetResult();
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result)
	{
		m_Result = _Result;
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> &_Result)
	{
		m_Result = _Result;
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result)
	{
		m_Result = _Result;
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result)
	{
		m_Result = _Result;
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result)
	{
		m_Result = fg_Move(_Result);
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport()
	{
		m_Result.f_SetResult();
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCAsyncResult<t_CReturnValue> const &_Result)
	{
		m_Result = _Result;
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCAsyncResult<t_CReturnValue> &_Result)
	{
		m_Result = _Result;
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCAsyncResult<t_CReturnValue> volatile &_Result)
	{
		m_Result = _Result;
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCAsyncResult<t_CReturnValue> const volatile &_Result)
	{
		m_Result = _Result;
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCAsyncResult<t_CReturnValue> &&_Result)
	{
		m_Result = fg_Move(_Result);
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCPromise<t_CReturnValue> &&_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = _Result.f_MoveResult();
		_Result.f_MoveFuture().f_DiscardResult();
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(TCPromise<t_CReturnValue> &&_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = _Result.f_MoveResult();
		f_OnResultPending();
		_Result.f_MoveFuture().f_DiscardResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetResult(tf_CResult &&_Result)
	{
		m_Result.f_SetResult(fg_Forward<tf_CResult>(_Result));
		f_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetResultNoReport(tf_CResult &&_Result)
	{
		m_Result.f_SetResult(fg_Forward<tf_CResult>(_Result));
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(tf_CResult &&_Result)
	{
		m_Result.f_SetException(fg_Forward<tf_CResult>(_Result));
		f_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> const &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		f_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		f_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> &&_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(fg_Move(_Result));
		f_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetCurrentException()
	{
		m_Result.f_SetCurrentException();
		f_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetExceptionNoReport(tf_CResult &&_Result)
	{
		m_Result.f_SetException(fg_Forward<tf_CResult>(_Result));
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetExceptionNoReport(TCAsyncResult<tf_CResult> const &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetExceptionNoReport(TCAsyncResult<tf_CResult> &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetExceptionNoReport(TCAsyncResult<tf_CResult> &&_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(fg_Move(_Result));
		f_OnResultPending();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetCurrentExceptionNoReportAppendable()
	{
		m_Result.f_SetExceptionAppendable(NException::fg_CurrentException());
		f_OnResultPendingAppendable();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetExceptionNoReportAppendable(NException::CExceptionPointer &&_pException)
	{
		m_Result.f_SetExceptionAppendable(fg_Move(_pException));
		f_OnResultPendingAppendable();
	}

	template <typename t_CReturnValue>
	TCFutureCoroutineKeepAlive<t_CReturnValue> TCPromiseData<t_CReturnValue>::f_CoroutineKeepAlive(TCActor<> &&_Actor)
	{
		DMibFastCheck(m_BeforeSuspend.m_bIsCoroutine);
		DMibFastCheck(_Actor);
		m_BeforeSuspend.f_NeedAtomicRead();

		if (m_Coroutine.promise().m_Flags & ECoroutineFlag_BreakSelfReference)
			return TCFutureCoroutineKeepAlive<t_CReturnValue>(_Actor.f_Weak(), fg_Explicit(this));
		else
			return TCFutureCoroutineKeepAlive<t_CReturnValue>(fg_Move(_Actor), fg_Explicit(this));
	}

	template <typename tf_CReturnValue>
	TCPromiseDataInit<tf_CReturnValue> fg_ConsumeFutureOnResultSet
		(
			CPromiseThreadLocal &_ThreadLocal
#if DMibEnableSafeCheck > 0
			, void const *_pConsumedBy
			, EConsumeFutureOnResultSetFlag _Flags
#endif
		)
	{
		if (_ThreadLocal.m_pOnResultSet)
		{
#if DMibEnableSafeCheck > 0
			DMibFastCheck(fg_GetTypeHash<tf_CReturnValue>() == _ThreadLocal.m_OnResultSetTypeHash);
			_ThreadLocal.m_pOnResultSetConsumedBy = _pConsumedBy;
			if (_Flags & EConsumeFutureOnResultSetFlag_ResetSafeCall)
				_ThreadLocal.m_bExpectCoroutineCall = false;
#endif
			return
				{
					.m_pOnResult = static_cast<TCFutureOnResult<tf_CReturnValue> *>(fg_Exchange(_ThreadLocal.m_pOnResultSet, nullptr))
					, .m_Flags = EFutureResultFlag_ResultFunctorSet
					, .m_bOnResultSetAtInit = true
				}
			;
		}

		return {};
	}

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
	{
		if constexpr (!(tf_Flags & EConsumePromiseFlag_FromCoroutine))
			DMibFastCheck(!_ThreadLocal.m_bSupendOnInitialSuspend); // It's not allowed to have a actor call that isn't a co-routine.

		NStorage::TCSharedPointer<NPrivate::TCPromiseData<tf_CType>> pReturn = [&]() inline_always_lambda -> NStorage::TCSharedPointer<NPrivate::TCPromiseData<tf_CType>>
			{
				if constexpr (tf_Flags & EConsumePromiseFlag_UsePromise)
				{
					if (_ThreadLocal.m_pUsePromise)
					{
#if DMibEnableSafeCheck > 0
						DMibFastCheck(fg_GetTypeHash<tf_CType>() == _ThreadLocal.m_UsePromiseTypeHash);
						_ThreadLocal.m_pUsePromiseConsumedBy = _pConsumedBy;
						if (_DebugFlags & EConsumePromiseDebugFlag_ResetSafeCall)
							_ThreadLocal.m_bExpectCoroutineCall = false;
#endif
						auto pPromise = static_cast<TCPromise<tf_CType> *>(fg_Exchange(_ThreadLocal.m_pUsePromise, nullptr));
						auto *pPromiseData = pPromise->mp_pData.f_Get();
						pPromiseData->m_BeforeSuspend.m_bPromiseConsumed = true;

						if constexpr (tf_Flags & EConsumePromiseFlag_FromCoroutine)
							pPromiseData->m_RefCount.f_PromiseToFuture();

						return fg_Move(pPromise->mp_pData);
					}
				}

				if constexpr (tf_Flags & EConsumePromiseFlag_HaveAllocation)
				{
					if (_pAllocation)
					{
						auto *pAllocated = new(_pAllocation) NPrivate::TCPromiseData<tf_CType>
							(
								_ThreadLocal
								, _RefCount
#if DMibEnableSafeCheck > 0
								, EConsumeFutureOnResultSetFlag_None
#endif
							)
						;
						pAllocated->m_BeforeSuspend.m_bAllocatedInCoroutineContext = true;
						return fg_Attach(pAllocated);
					}
				}

				return fg_Construct(_ThreadLocal, _RefCount);
			}
			()
		;

		return pReturn;
	}

	template <typename tf_CType, typename tf_CAllocator>
	void fg_DeleteObject(tf_CAllocator &&_Allocator, TCPromiseData<tf_CType> *_pObject)
	{
		_pObject->~TCPromiseData<tf_CType>();
		umint Size = sizeof(TCPromiseData<tf_CType>);
		if (_pObject->m_BeforeSuspend.m_bAllocatedInCoroutineContext)
		{
			uint8 *pEndMemory = (uint8 *)_pObject->m_Coroutine.address();
			uint8 *pMemory = (uint8 *)_pObject;
			Size = pEndMemory - pMemory;
		}

		fg_Forward<tf_CAllocator>(_Allocator).f_Free(_pObject, Size);
	}
}
