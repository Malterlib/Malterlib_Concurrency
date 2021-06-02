// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture()
	{
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &_pData)
		: mp_pData(_pData)
	{
		DMibFastCheck(mp_pData->m_bFutureGotten);
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&_pData)
		: mp_pData(fg_Move(_pData))
	{
		DMibFastCheck(mp_pData->m_bFutureGotten);
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &TCFuture<t_CReturnValue>::operator =(TCFuture &&_Other)
	{
		mp_pData = fg_Move(_Other.mp_pData);
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
		this->mp_pData = _Other.mp_pData;
		return *this;
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &TCPromise<t_CReturnValue>::operator = (TCPromise &&_Other)
	{
		this->mp_pData = fg_Move(_Other.mp_pData);
		return *this;
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise()
		: mp_pData(fg_Construct())
	{
	}

	enum EFutureResultFlag
 	{
		EFutureResultFlag_None = 0
		, EFutureResultFlag_DataSet = DMibBit(0)
		, EFutureResultFlag_ResultFunctorSet = DMibBit(1)
		, EFutureResultFlag_DiscardResult = DMibBit(2)
	};

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_Future() const
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(mp_pData);
		DMibFastCheck(!mp_pData->m_bFutureGotten); // A promise can only be used for one future
		mp_pData->m_bFutureGotten = true;
#endif
		return mp_pData;
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_MoveFuture()
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(mp_pData);
		DMibFastCheck(!mp_pData->m_bFutureGotten); // A promise can only be used for one future
		mp_pData->m_bFutureGotten = true;
#endif
		return fg_Move(mp_pData);
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_IsSet() const
	{
		return (this->mp_pData->m_Result).f_IsSet();
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
	template <typename tf_CResult, TCEnableIfType<!NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CResult>::CType, NException::CExceptionBase>::mc_Value> *>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (tf_CResult &&_Result)
	{
		this->mp_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> *>
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
	template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall)
	{
		fg_Move(_ActorCall) > *this;
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	mark_no_coroutine_debug TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::operator <<= (TCFuture<t_CReturnValue> &&_Future)
	{
		fg_Move(_Future) > *this;
		return f_MoveFuture();
	}

	template <typename t_CReturnValue>
	auto TCPromise<t_CReturnValue>::f_ReceiveAny() const -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<t_CReturnValue>>
	{
		return NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<t_CReturnValue>>{*this};
	}

	template <>
	auto TCPromise<void>::f_ReceiveAny() const -> NPrivate::TCPromiseReceiveAnyFunctor<TCPromise<void>>;

	namespace NPrivate
	{
		template <typename t_CReturnValue, typename t_CException>
		template <typename tf_FToRun>
		TCAsyncResult<t_CReturnValue> TCRunProtectedHelper<t_CReturnValue, t_CException>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCAsyncResult<t_CReturnValue> Result;
			if constexpr (NTraits::TCIsVoid<t_CReturnValue>::mc_Value)
			{
				if constexpr (NTraits::TCIsVoid<t_CException>::mc_Value)
				{
					try
					{
						_ToRun();
						Result.f_SetResult();
					}
					catch (...)
					{
						Result.f_SetCurrentException();
					}
				}
				else
				{
					try
					{
						_ToRun();
						Result.f_SetResult();
					}
					catch (t_CException const &)
					{
						Result.f_SetCurrentException();
					}
				}
			}
			else
			{
				if constexpr (NTraits::TCIsVoid<t_CException>::mc_Value)
				{
					try
					{
						Result.f_SetResult(_ToRun());
					}
					catch (...)
					{
						Result.f_SetCurrentException();
					}
				}
				else
				{
					try
					{
						Result.f_SetResult(_ToRun());
					}
					catch (t_CException const &)
					{
						Result.f_SetCurrentException();
					}
				}
			}
			return Result;
		}

		template <typename t_CReturnValue, typename t_CException>
		template <typename tf_FToRun, typename ...tfp_CParams>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<t_CReturnValue, t_CException>::operator () (tf_FToRun &&_ToRun, tfp_CParams && ...p_Params) const
		{
			TCPromise<t_CReturnValue> Result;
			NException::CDisableExceptionTraceScope DisableTrace;
			if constexpr (NTraits::TCIsVoid<t_CReturnValue>::mc_Value)
			{
				if constexpr (NTraits::TCIsVoid<t_CException>::mc_Value)
				{
					try
					{
						_ToRun(fg_Forward<tfp_CParams>(p_Params)...);
						Result.f_SetResult();
					}
					catch (...)
					{
						Result.f_SetCurrentException();
					}
				}
				else
				{
					try
					{
						_ToRun(fg_Forward<tfp_CParams>(p_Params)...);
						Result.f_SetResult();
					}
					catch (t_CException const &)
					{
						Result.f_SetCurrentException();
					}
				}
			}
			else
			{
				if constexpr (NTraits::TCIsVoid<t_CException>::mc_Value)
				{
					try
					{
						Result.f_SetResult(_ToRun(fg_Forward<tfp_CParams>(p_Params)...));
					}
					catch (...)
					{
						Result.f_SetCurrentException();
					}
				}
				else
				{
					try
					{
						Result.f_SetResult(_ToRun(fg_Forward<tfp_CParams>(p_Params)...));
					}
					catch (t_CException const &)
					{
						Result.f_SetCurrentException();
					}
				}
			}
			return Result.f_MoveFuture();
		}

		template <typename t_CReturnValue, typename t_CException>
		template <typename tf_FToRun>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<t_CReturnValue, t_CException>::operator / (tf_FToRun &&_ToRun) const
		{
			return (*this)(fg_Forward<tf_FToRun>(_ToRun));
		}
	}

	template <typename t_CReturnValue>
	NPrivate::TCRunProtectedHelper<t_CReturnValue> TCFuture<t_CReturnValue>::fs_RunProtectedAsyncResult()
	{
		return NPrivate::TCRunProtectedHelper<t_CReturnValue>();
	}

	template <typename t_CReturnValue>
	template <typename tf_CException>
	NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> TCFuture<t_CReturnValue>::fs_RunProtectedAsyncResult()
	{
		return NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException>();
	}

	template <typename t_CReturnValue>
	NPrivate::TCRunProtectedFutureHelper<t_CReturnValue> TCFuture<t_CReturnValue>::fs_RunProtected()
	{
		return NPrivate::TCRunProtectedFutureHelper<t_CReturnValue>();
	}

	template <typename t_CReturnValue>
	template <typename tf_CException>
	NPrivate::TCRunProtectedFutureHelper<t_CReturnValue, tf_CException> TCFuture<t_CReturnValue>::fs_RunProtected()
	{
		return NPrivate::TCRunProtectedFutureHelper<t_CReturnValue, tf_CException>();
	}

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
				mc_Value = NMeta::TCAllIsTrue
				<
					NTraits::TCIsVoid<tp_CParam>::mc_Value...
				>::mc_Value
			};
		};

		template <typename t_FFunctor>
		struct TCAllAsyncResultsAreVoid
		{
			enum
			{
				mc_Value = TCAllAsyncResultsAreVoidHelper
				<
					typename NTraits::TCFunctionTraits
					<
						typename NTraits::NPrivate::TCFunctionObjectType_Helper
						<
							typename NTraits::TCRemoveReference<t_FFunctor>::CType
						>::CType
					>::CParams
				>::mc_Value
			};
		};
	}

	template <typename t_CReturnValue>
	template
	<
		typename tf_FResultHandler
		, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *
	>
	auto TCPromise<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[
							&
						]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(fg_Move(p_Results));
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				if (bFailed)
					return;
				(void)Dummy;
				fResultHandler();
			}
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromise<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(fg_Move(p_Results));
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				if (bFailed)
					return;
				fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	template <typename t_CReturnValue>
	auto TCPromise<t_CReturnValue>::operator % (NStr::CStr const &_ErrorString) const -> TCPromiseWithError<t_CReturnValue>
	{
		return TCPromiseWithError<t_CReturnValue>(*this, _ErrorString);
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator % (NStr::CStr const &_ErrorString) && -> TCFutureWithError<t_CReturnValue>
	{
		return TCFutureWithError<t_CReturnValue>(fg_Move(*this), _ErrorString);
	}

	template <typename t_CReturnValue>
	TCPromiseWithError<t_CReturnValue>::TCPromiseWithError(TCPromise<t_CReturnValue> const &_Promise, NStr::CStr const &_Error)
		: CActorWithErrorBase{_Error}
		, m_Promise(_Promise)
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Error = this->m_Error]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(DMibErrorInstance(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				if (bFailed)
					return;
				fResultHandler();
			}
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
	auto TCPromiseWithError<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
	{
		return [Promise = m_Promise, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Error = this->m_Error]
			(auto &&...p_Results) mutable
			{
				bool bFailed = false;
				TCInitializerList<bool> Dummy =
					{
						[&]
						{
							if (!bFailed && !p_Results)
							{
								Promise.f_SetException(DMibErrorInstance(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
								bFailed = true;
							}
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				if (bFailed)
					return;
				fResultHandler(fg_Move(*p_Results)...);
			}
		;
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_OnResultSetOrAvailable(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult)
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(!pData->m_bOnResultSetAtInit);

		pData->m_fOnResult = fg_Move(_fOnResult);

		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_ResultFunctorSet)); // You can only observe the result once

		if (PreviousFlags & EFutureResultFlag_DataSet)
		{
			pData->m_fOnResult.f_Clear();
			return true;
		}

		return false;
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult)
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(!pData->m_bOnResultSetAtInit);

		pData->m_fOnResult = fg_Move(_fOnResult);

		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_ResultFunctorSet);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_ResultFunctorSet)); // You can only observe the result once

		if (PreviousFlags & EFutureResultFlag_DataSet)
		{
			pData->m_fOnResult(fg_Move(pData->m_Result));
			pData->m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_IsCoroutine() const
	{
		return mp_pData->m_bIsCoroutine;
	}

	template <typename t_CReturnValue>
	ECoroutineFlag TCFuture<t_CReturnValue>::f_CoroutineFlags() const
	{
		if (!mp_pData->m_Coroutine)
			return ECoroutineFlag_None;

		return mp_pData->m_Coroutine.promise().m_Flags;
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_DiscardResult()
	{
		auto pData = mp_pData.f_Get();
		pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_DiscardResult, pData->f_MemoryOrder());
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_ObserveIfAvailable()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(!pData->m_bOnResultSetAtInit);

		mint Expected = EFutureResultFlag_DataSet;
		return pData->m_OnResultSet.f_CompareExchangeWeak(Expected, EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult);
	}

	template <typename t_CReturnValue>
	TCAsyncResult<t_CReturnValue> &&TCFuture<t_CReturnValue>::f_MoveResult()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_OnResultSet.f_Load(pData->f_MemoryOrder()) & EFutureResultFlag_DataSet);
		return fg_Move(pData->m_Result);
	}

	template <typename t_CReturnValue>
	TCAsyncResult<t_CReturnValue> &&TCPromise<t_CReturnValue>::f_MoveResult()
	{
		auto pData = mp_pData.f_Get();
		DMibFastCheck(pData->m_OnResultSet.f_Load(pData->f_MemoryOrder()) & EFutureResultFlag_DataSet);
		return fg_Move(pData->m_Result);
	}

	template <typename t_CReturnValue>
	template <typename tf_FOnEmpty>
	void TCPromise<t_CReturnValue>::f_Abandon(tf_FOnEmpty &&_fOnEmpty)
	{
		auto pData = mp_pData.f_Get();
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet, pData->f_MemoryOrder());
		if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet)) == EFutureResultFlag_ResultFunctorSet)
		{
			pData->m_Result.f_SetException(_fOnEmpty());
			pData->m_fOnResult(fg_Move(pData->m_Result));
			pData->m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_IsObserved() const
	{
		auto pData = mp_pData.f_Get();
		return (pData->m_OnResultSet.f_Load(pData->f_MemoryOrder()) & (EFutureResultFlag_ResultFunctorSet | EFutureResultFlag_DiscardResult)) != EFutureResultFlag_None;
	}

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
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::TCPromiseData()
		: m_fOnResult
		(
			fg_ConsumeFutureOnResultSet<t_CReturnValue>
			(
#if DMibEnableSafeCheck > 0
				static_cast<CPromiseDataBase const *>(this)
				, EConsumeFutureOnResultSetFlag_ResetSafeCall
#endif
			)
		)
		, m_OnResultSet(m_fOnResult ? EFutureResultFlag_ResultFunctorSet : EFutureResultFlag_None)
		, m_bOnResultSetAtInit(m_fOnResult)
#if DMibConfig_Concurrency_DebugFutures
		, CPromiseDataBase(fg_GetTypeName<t_CReturnValue>())
#endif
	{
#if DMibConfig_Concurrency_DebugUnobservedException
		m_Callstack.m_CallstackLen = NSys::fg_System_GetStackTrace(m_Callstack.m_Callstack, 128);
#endif
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_Reset()
	{
		m_fOnResult = fg_ConsumeFutureOnResultSet<t_CReturnValue>
			(
#if DMibEnableSafeCheck > 0
				static_cast<CPromiseDataBase const *>(this)
				, EConsumeFutureOnResultSetFlag_ResetSafeCall
#endif
			)
		;
		m_bOnResultSetAtInit = !!m_fOnResult;
		m_Result = TCAsyncResult<t_CReturnValue>();
		m_OnResultSet = m_fOnResult ? EFutureResultFlag_ResultFunctorSet : EFutureResultFlag_None;
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::fp_ReportNothingSet()
	{
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet, f_MemoryOrder());
		if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet)) == EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result)); // Report nothing set
			m_fOnResult.f_Clear();
		}
		else if (((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet | EFutureResultFlag_DiscardResult)) == EFutureResultFlag_DataSet) && !m_bIsGenerator)
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
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = **g_SystemThreadLocal;

		if
			(
			 	ThreadLocal.m_PromiseThreadLocal.m_bCaptureDebugException
			 	&& !(m_OnResultSet.f_Load() & EFutureResultFlag_DataSet)
			 	&& NException::fg_UncaughtExceptions()
			 	&& ThreadLocal.m_PromiseThreadLocal.m_pOnResultSetConsumedBy == (void *)this
			)
		{
			NException::CDisableExceptionFilterScope DisableExceptionFilter;

			try
			{
				std::rethrow_exception(NException::fg_CurrentException());
			}
			catch (NException::CDebugException const &)
			{
				f_SetException(NException::fg_CurrentException());
			}
			catch (...)
			{
			}
		}
#endif

		try
		{
			fp_ReportNothingSet();
		}
		catch (...)
		{
		}

		if (m_Coroutine)
		{
#if DMibEnableSafeCheck > 0
			auto Owner = m_CoroutineOwner.f_Lock();
			DMibFastCheck(Owner && Owner->f_CurrentlyProcessing());
#endif
			m_Coroutine.destroy();
		}
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::fp_OnResult()
	{
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet, f_MemoryOrder());
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once
		DMibFastCheck(m_Result.f_IsSet());

		if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
	}

	template <typename t_CReturnValue>
	NAtomic::EMemoryOrder TCPromiseData<t_CReturnValue>::f_MemoryOrder(NAtomic::EMemoryOrder _Default) const
	{
		return m_bOnResultSetAtInit ? NAtomic::EMemoryOrder_Relaxed : _Default;
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult()
	{
		m_Result.f_SetResult();
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result)
	{
		m_Result = _Result;
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> &_Result)
	{
		m_Result = _Result;
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result)
	{
		m_Result = _Result;
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result)
	{
		m_Result = _Result;
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result)
	{
		m_Result = fg_Move(_Result);
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCPromise<t_CReturnValue> &&_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = _Result.f_MoveResult();
		_Result.f_MoveFuture().f_DiscardResult();
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetResult(tf_CResult &&_Result)
	{
		m_Result.f_SetResult(fg_Forward<tf_CResult>(_Result));
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(tf_CResult &&_Result)
	{
		m_Result.f_SetException(fg_Forward<tf_CResult>(_Result));
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> const &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> &_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(_Result);
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromiseData<t_CReturnValue>::f_SetException(TCAsyncResult<tf_CResult> &&_Result)
	{
		DMibRequire(!_Result);
		m_Result.f_SetException(fg_Move(_Result));
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetCurrentException()
	{
		m_Result.f_SetCurrentException();
		fp_OnResult();
	}

	template <typename tf_CReturnValue>
	NFunction::TCFunctionMovable<void (TCAsyncResult<tf_CReturnValue> &&_AsyncResult)> fg_ConsumeFutureOnResultSet
		(
#if DMibEnableSafeCheck > 0
		 	void const *_pConsumedBy
			, EConsumeFutureOnResultSetFlag _Flags
#endif
		)
	{
		auto &ThreadLocal = **g_SystemThreadLocal;
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;
		if (PromiseThreadLocal.m_pOnResultSet)
		{
#if DMibEnableSafeCheck > 0
			DMibFastCheck(fg_GetTypeHash<tf_CReturnValue>() == PromiseThreadLocal.m_OnResultSetTypeHash);
			PromiseThreadLocal.m_pOnResultSetConsumedBy = _pConsumedBy;
			if (_Flags & EConsumeFutureOnResultSetFlag_ResetSafeCall)
				ThreadLocal.m_bExpectCoroutineCall = false;
#endif
			return fg_Move(*static_cast<NFunction::TCFunctionMovable<void (TCAsyncResult<tf_CReturnValue> &&_AsyncResult)> *>(fg_Exchange(PromiseThreadLocal.m_pOnResultSet, nullptr)));
		}
		return nullptr;
	}
}
