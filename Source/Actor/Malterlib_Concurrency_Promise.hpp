// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCFuture const &_Other)
		: m_pData(_Other.m_pData)
	{
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &TCFuture<t_CReturnValue>::operator =(TCFuture const &_Other)
	{
		m_pData = _Other.m_pData;
		return *this;
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &TCFuture<t_CReturnValue>::operator =(TCFuture &&_Other)
	{
		m_pData = fg_Move(_Other.m_pData);
		return *this;
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCFuture &&_Other)
		: m_pData(fg_Move(_Other.m_pData))
	{
	}
	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture()
		: m_pData(fg_Construct())
	{
	}

	template <typename t_CReturnValue>
	template <typename tf_CType>
	TCFuture<t_CReturnValue>::TCFuture(TCExplicit<tf_CType> &&_Data)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetResult(*_Data);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCExplicit<void> &&_Data)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetResult();
	}

	template <typename t_CReturnValue>
	template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> *>
	TCFuture<t_CReturnValue>::TCFuture(tf_CType &&_Exception)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetException(fg_Forward<tf_CType>(_Exception));
	}


	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(NException::CExceptionPointer const &_pException)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetException(_pException);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCAsyncResult<t_CReturnValue> const &_Result)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetResult(_Result);
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue>::TCFuture(TCAsyncResult<t_CReturnValue> &&_Result)
		: m_pData(fg_Construct())
	{
		m_pData->f_SetResult(fg_Move(_Result));
	}

	template <typename t_CReturnValue>
	template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
	TCFuture<t_CReturnValue>::TCFuture(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall)
		: m_pData(fg_Construct())
	{
		TCPromise<t_CReturnValue> Promise(*this);
		fg_Move(_ActorCall) > Promise;
	}


	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCFuture<t_CReturnValue> const &_Other)
		: TCFuture<t_CReturnValue>(_Other)
	{
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCPromise const &_Other)
		: TCFuture<t_CReturnValue>(_Other)
	{
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &TCPromise<t_CReturnValue>::operator =(TCPromise const &_Other)
	{
		this->m_pData = _Other.m_pData;
		return *this;
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue> &TCPromise<t_CReturnValue>::operator =(TCPromise &&_Other)
	{
		this->m_pData = fg_Move(_Other.m_pData);
		return *this;
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCPromise &&_Other)
		: TCFuture<t_CReturnValue>(fg_Move(_Other))
	{
	}
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise()
	{
	}
	
	template <typename t_CReturnValue>
	template <typename tf_CType>
	TCPromise<t_CReturnValue>::TCPromise(TCExplicit<tf_CType> &&_Data)
	{
		f_SetResult(*_Data);
	}

	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCExplicit<void> &&_Data)
	{
		f_SetResult();
	}
	
	template <typename t_CReturnValue>
	template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> *>
	TCPromise<t_CReturnValue>::TCPromise(tf_CType &&_Exception)
	{
		f_SetException(fg_Forward<tf_CType>(_Exception));
	}
	
	
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(NException::CExceptionPointer const &_pException)
	{
		f_SetException(_pException);
	}
	
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCAsyncResult<t_CReturnValue> const &_Result)
	{
		f_SetResult(_Result);
	}
	
	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::TCPromise(TCAsyncResult<t_CReturnValue> &&_Result)
	{
		f_SetResult(fg_Move(_Result));
	}
	
	template <typename t_CReturnValue>
	template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, bool tf_bDirectCall>
	TCPromise<t_CReturnValue>::TCPromise(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList, tf_bDirectCall> &&_ActorCall)
	{
		fg_Move(_ActorCall) > *this;
	}

	enum EFutureResultFlag
	{
		EFutureResultFlag_None = 0
		, EFutureResultFlag_DataSet = DMibBit(0)
		, EFutureResultFlag_ResultFunctorSet = DMibBit(1)
		, EFutureResultFlag_DiscardResult = DMibBit(2)
	};

/*	template <typename t_CReturnValue>
	TCPromise<t_CReturnValue>::operator bool () const
	{
		return !!(m_pData->m_Result);
	}*/

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> TCPromise<t_CReturnValue>::f_Future() const
	{
		return *this;
	}

	template <typename t_CReturnValue>
	TCFuture<t_CReturnValue> &&TCPromise<t_CReturnValue>::f_MoveFuture()
	{
		return fg_Move(*this);
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_IsSet() const
	{
		return (this->m_pData->m_Result).f_IsSet();
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_SetResult() const
	{
		this->m_pData->f_SetResult();
	}
	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetResult(tf_CResult &&_Result) const
	{
		this->m_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
	}
	template <typename t_CReturnValue>
	template <typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetException(tf_CResult &&_Result) const
	{
		this->m_pData->f_SetException(fg_Forward<tf_CResult>(_Result));
	}

	template <typename t_CReturnValue>
	template <typename tf_CException, typename tf_CResult>
	void TCPromise<t_CReturnValue>::f_SetExceptionOrResult(tf_CException &&_Exception, tf_CResult &&_Result) const
	{
		if (!_Exception)
			this->m_pData->f_SetException(fg_Forward<tf_CException>(_Exception));
		else
			this->m_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
	}

	template <typename t_CReturnValue>
	void TCPromise<t_CReturnValue>::f_SetCurrentException() const
	{
		this->m_pData->f_SetCurrentException();
	}

	namespace NPrivate
	{
		template <typename t_CPromise>
		struct TCPromiseReceiveAnyFunctor
		{
			TCPromiseReceiveAnyFunctor(t_CPromise const &_Promise)
				: m_Promise(_Promise)
			{
			}
			
			template <typename ...tfp_CResult>
			void operator () (tfp_CResult && ...p_Result);
			
			t_CPromise m_Promise;
		};
		
		template <typename t_CPromise>
		template <typename ...tfp_CResult>
		void TCPromiseReceiveAnyFunctor<t_CPromise>::operator () (tfp_CResult && ...p_Result)
		{
			if (m_Promise.f_IsSet())
				return;
			
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						if (m_Promise.f_IsSet())
							return true;
						if (!p_Result)
							m_Promise.f_SetException(fg_Move(p_Result));
						return true;
					}
					()...
				}
			;
			(void)Dummy;
			if (m_Promise.f_IsSet())
				return;
			
			TCInitializerList<bool> Dummy2 = 
				{
					[&]
					{
						if (m_Promise.f_IsSet())
							return true;
						if (p_Result)
							m_Promise.f_SetResult(fg_Move(*p_Result));
						return true;
					}
					()...
				}
			;
			(void)Dummy2;
		}

		template <>
		template <typename ...tfp_CResult>
		void TCPromiseReceiveAnyFunctor<TCPromise<void>>::operator () (tfp_CResult && ...p_Result)
		{
			if (m_Promise.f_IsSet())
				return;
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						if (m_Promise.f_IsSet())
							return true;
						if (!p_Result)
							m_Promise.f_SetException(fg_Move(p_Result));
						return true;
					}
					()...
				}
			;
			(void)Dummy;
			if (m_Promise.f_IsSet())
				return;
			m_Promise.f_SetResult();
		}
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
			try
			{
				Result.f_SetResult(_ToRun());
			}
			catch (t_CException const &)
			{
				Result.f_SetCurrentException();
			}
			return Result;
		}

		template <typename t_CReturnValue>
		template <typename tf_FToRun>
		TCAsyncResult<t_CReturnValue> TCRunProtectedHelper<t_CReturnValue, void>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCAsyncResult<t_CReturnValue> Result;
			try
			{
				Result.f_SetResult(_ToRun());
			}
			catch (...)
			{
				Result.f_SetCurrentException();
			}
			return Result;
		}

		template <typename t_CException>
		template <typename tf_FToRun>
		TCAsyncResult<void> TCRunProtectedHelper<void, t_CException>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCAsyncResult<void> Result;
			try
			{
				_ToRun();
				Result.f_SetResult();
			}
			catch (t_CException const &)
			{
				Result.f_SetCurrentException();
			}
			return Result;
		}

		template <typename tf_FToRun>
		TCAsyncResult<void> TCRunProtectedHelper<void, void>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCAsyncResult<void> Result;
			try
			{
				_ToRun();
				Result.f_SetResult();
			}
			catch (...)
			{
				Result.f_SetCurrentException();
			}
			return Result;
		}

		template <typename t_CReturnValue, typename t_CException>
		template <typename tf_FToRun>
		TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<t_CReturnValue, t_CException>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCPromise<t_CReturnValue> Result;
			try
			{
				Result.f_SetResult(_ToRun());
			}
			catch (t_CException const &)
			{
				Result.f_SetCurrentException();
			}
			return Result.f_MoveFuture();
		}

		template <typename t_CReturnValue>
		template <typename tf_FToRun>
		TCFuture<t_CReturnValue> TCRunProtectedFutureHelper<t_CReturnValue, void>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCPromise<t_CReturnValue> Result;
			try
			{
				Result.f_SetResult(_ToRun());
			}
			catch (...)
			{
				Result.f_SetCurrentException();
			}
			return Result.f_MoveFuture();
		}

		template <typename t_CException>
		template <typename tf_FToRun>
		TCFuture<void> TCRunProtectedFutureHelper<void, t_CException>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCPromise<void> Result;
			try
			{
				_ToRun();
				Result.f_SetResult();
			}
			catch (t_CException const &)
			{
				Result.f_SetCurrentException();
			}
			return Result.f_MoveFuture();
		}

		template <typename tf_FToRun>
		TCFuture<void> TCRunProtectedFutureHelper<void, void>::operator / (tf_FToRun &&_ToRun) const
		{
			NException::CDisableExceptionTraceScope DisableTrace;
			TCPromise<void> Result;
			try
			{
				_ToRun();
				Result.f_SetResult();
			}
			catch (...)
			{
				Result.f_SetCurrentException();
			}
			return Result.f_MoveFuture();
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
#if defined(DCompiler_MSVC_Workaround)
							, &Promise
#endif
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
		auto pData = m_pData.f_Get();
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
		auto pData = m_pData.f_Get();
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
		return !!m_pData->m_Coroutine;
	}

	template <typename t_CReturnValue>
	ECoroutineFlag TCFuture<t_CReturnValue>::f_CoroutineFlags() const
	{
		if (!m_pData->m_Coroutine)
			return ECoroutineFlag_None;

		return m_pData->m_Coroutine.promise().m_Flags;
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::f_DiscardResult()
	{
		auto pData = m_pData.f_Get();
		pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_DiscardResult);// , NAtomic::EMemoryOrder_Relaxed);
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_ObserveIfAvailable()
	{
		auto pData = m_pData.f_Get();
		mint Expected = EFutureResultFlag_DataSet;
		return pData->m_OnResultSet.f_CompareExchangeWeak(Expected, EFutureResultFlag_DataSet | EFutureResultFlag_DiscardResult);
	}

	template <typename t_CReturnValue>
	template <typename tf_FOnEmpty>
	void TCFuture<t_CReturnValue>::f_Abandon(tf_FOnEmpty &&_fOnEmpty)
	{
		auto pData = m_pData.f_Get();
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)pData->m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet);
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
		auto pData = m_pData.f_Get();
		return (pData->m_OnResultSet.f_Load() & (EFutureResultFlag_ResultFunctorSet | EFutureResultFlag_DiscardResult)) != EFutureResultFlag_None;
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
	void TCPromiseData<t_CReturnValue>::fp_ReportNothingSet()
	{
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet);
		if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet)) == EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result)); // Report nothing set
			m_fOnResult.f_Clear();
		}
		else if ((PreviousFlags & (EFutureResultFlag_DataSet | EFutureResultFlag_ResultFunctorSet | EFutureResultFlag_DiscardResult)) == EFutureResultFlag_DataSet)
		{
			if (!m_Result)
				fg_ReportUnobservedException(m_Result.f_GetException());
		}
	}
	
	template <typename t_CReturnValue>
	TCPromiseData<t_CReturnValue>::~TCPromiseData()
	{
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
		EFutureResultFlag PreviousFlags = (EFutureResultFlag)m_OnResultSet.f_FetchOr(EFutureResultFlag_DataSet);
		DMibFastCheck(!(PreviousFlags & EFutureResultFlag_DataSet)); // You can only set result once
		DMibFastCheck(m_Result.f_IsSet());

		if (PreviousFlags & EFutureResultFlag_ResultFunctorSet)
		{
			m_fOnResult(fg_Move(m_Result));
			m_fOnResult.f_Clear();
		}
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
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCPromise<t_CReturnValue> const &_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = _Result.m_pData->m_Result;
		_Result.f_DiscardResult();
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCPromise<t_CReturnValue> &_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = _Result.m_pData->m_Result;
		_Result.f_DiscardResult();
		fp_OnResult();
	}

	template <typename t_CReturnValue>
	void TCPromiseData<t_CReturnValue>::f_SetResult(TCPromise<t_CReturnValue> &&_Result)
	{
		DMibFastCheck(_Result.f_IsSet());
		m_Result = fg_Move(_Result.m_pData->m_Result);
		_Result.f_DiscardResult();
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
}
