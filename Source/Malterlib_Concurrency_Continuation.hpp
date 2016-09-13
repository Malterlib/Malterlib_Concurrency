// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation(TCContinuation const &_Other)
			: m_pData(_Other.m_pData)
		{
		}
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue> &TCContinuation<t_CReturnValue>::operator =(TCContinuation const &_Other)
		{
			m_pData = _Other.m_pData;
			return *this;
		}
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue> &TCContinuation<t_CReturnValue>::operator =(TCContinuation &&_Other)
		{
			m_pData = fg_Move(_Other.m_pData);
			return *this;
		}
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation(TCContinuation &&_Other)
			: m_pData(fg_Move(_Other.m_pData))
		{
		}
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation()
			: m_pData(fg_Construct())
		{
		}
		
		template <typename t_CReturnValue>
		template <typename tf_CType>
		TCContinuation<t_CReturnValue>::TCContinuation(TCExplicit<tf_CType> &&_Data)
			: m_pData(fg_Construct())
		{
			f_SetResult(*_Data);
		}

		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation(TCExplicit<void> &&_Data)
			: m_pData(fg_Construct())
		{
			f_SetResult();
		}
		
		template <typename t_CReturnValue>
		template <typename tf_CType, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tf_CType>::CType, NException::CExceptionBase>::mc_Value> *>
		TCContinuation<t_CReturnValue>::TCContinuation(tf_CType &&_Exception)
			: m_pData(fg_Construct())
		{
			f_SetException(fg_Forward<tf_CType>(_Exception));
		}
		
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation(TCAsyncResult<t_CReturnValue> const &_Result)
			: m_pData(fg_Construct())
		{
			f_SetResult(_Result);
		}
		
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::TCContinuation(TCAsyncResult<t_CReturnValue> &&_Result)
			: m_pData(fg_Construct())
		{
			f_SetResult(fg_Move(_Result));
		}
		
		template <typename t_CReturnValue>
		template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList>
		TCContinuation<t_CReturnValue>::TCContinuation(TCActorCall<tf_CActor, tf_CFunctor, tf_CParams, tf_CTypeList> &&_ActorCall)
			: m_pData(fg_Construct())
		{
			_ActorCall > *this;
		}
		
		template <typename t_CReturnValue>
		template <typename tf_CResult>
		TCContinuation<t_CReturnValue> TCContinuation<t_CReturnValue>::fs_Finished(tf_CResult &&_Result)
		{
			TCContinuation Ret;
			Ret.f_SetResult(fg_Forward<tf_CResult>(_Result));
			return Ret;
		}
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue> TCContinuation<t_CReturnValue>::fs_Finished()
		{
			TCContinuation Ret;
			Ret.f_SetResult();
			return Ret;
		}
		
		enum EContinuationResultFlag
		{
			EContinuationResultFlag_DataSet = DMibBit(0)
			, EContinuationResultFlag_ResultFunctorSet = DMibBit(1)
		};

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::fp_ReportNothingSet()
		{
			if 
				(
					(
						m_OnResultSet.f_FetchOr(EContinuationResultFlag_DataSet) 
						& (EContinuationResultFlag_DataSet | EContinuationResultFlag_ResultFunctorSet)
					) 
					== EContinuationResultFlag_ResultFunctorSet
				)
			{
				m_OnResult(fg_Move(m_Result)); // Report nothing set
				m_OnResult.f_Clear();
			}
		}
		
		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::CData::~CData()
		{
			try
			{
				fp_ReportNothingSet();
			}
			catch (...)
			{
			}
		}

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::fp_OnResult()
		{
			if (m_OnResultSet.f_FetchOr(EContinuationResultFlag_DataSet) & EContinuationResultFlag_ResultFunctorSet)
			{
				m_OnResult(fg_Move(m_Result));
				m_OnResult.f_Clear();
			}
		}

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::f_OnResultSet(NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturnValue> &&_AsyncResult)> &&_fOnResult)
		{
			auto pData = m_pData.f_Get();
			pData->m_OnResult = fg_Move(_fOnResult);
			if (pData->m_OnResultSet.f_FetchOr(EContinuationResultFlag_ResultFunctorSet) & EContinuationResultFlag_DataSet)
			{
				pData->m_OnResult(fg_Move(pData->m_Result));
				pData->m_OnResult.f_Clear();
			}
		}
		
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult()
		{
			m_Result.f_SetResult();
			fp_OnResult();
		}
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(TCAsyncResult<t_CReturnValue> const &_Result)
		{
			m_Result = _Result;
			fp_OnResult();
		}
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(TCAsyncResult<t_CReturnValue> &_Result)
		{
			m_Result = _Result;
			fp_OnResult();
		}
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(TCAsyncResult<t_CReturnValue> volatile &_Result)
		{
			m_Result = _Result;
			fp_OnResult();
		}
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(TCAsyncResult<t_CReturnValue> const volatile &_Result)
		{
			m_Result = _Result;
			fp_OnResult();
		}
		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(TCAsyncResult<t_CReturnValue> &&_Result)
		{
			m_Result = fg_Move(_Result);
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::CData::f_SetResult(tf_CResult &&_Result)
		{
			m_Result.f_SetResult(fg_Forward<tf_CResult>(_Result));
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::CData::f_SetException(tf_CResult &&_Result)
		{
			m_Result.f_SetException(fg_Forward<tf_CResult>(_Result));
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::CData::f_SetException(TCAsyncResult<tf_CResult> const &_Result)
		{
			DMibRequire(!_Result);
			m_Result.f_SetException(_Result);
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::CData::f_SetException(TCAsyncResult<tf_CResult> &_Result)
		{
			DMibRequire(!_Result);
			m_Result.f_SetException(_Result);
			fp_OnResult();
		}
		
		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::CData::f_SetException(TCAsyncResult<tf_CResult> &&_Result)
		{
			DMibRequire(!_Result);
			m_Result.f_SetException(fg_Move(_Result));
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::f_SetCurrentException()
		{
			m_Result.f_SetCurrentException();
			fp_OnResult();
		}

		template <typename t_CReturnValue>
		TCContinuation<t_CReturnValue>::operator bool () const
		{
			return !!(m_pData->m_Result);
		}

		template <typename t_CReturnValue>
		bool TCContinuation<t_CReturnValue>::f_IsSet() const
		{
			return (m_pData->m_Result).f_IsSet();
		}

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::f_SetResult() const
		{
			m_pData->f_SetResult();
		}
		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::f_SetResult(tf_CResult &&_Result) const
		{
			m_pData->f_SetResult(fg_Forward<tf_CResult>(_Result));
		}
		template <typename t_CReturnValue>
		template <typename tf_CResult>
		void TCContinuation<t_CReturnValue>::f_SetException(tf_CResult &&_Result) const
		{
			m_pData->f_SetException(fg_Forward<tf_CResult>(_Result));
		}

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::f_SetCurrentException() const
		{
			m_pData->f_SetCurrentException();
		}

		namespace NPrivate
		{
			
			template <typename t_CReturnValue, typename t_CException>
			template <typename tf_FToRun>
			TCContinuation<t_CReturnValue> TCRunProtectedHelper<t_CReturnValue, t_CException>::operator > (tf_FToRun &&_ToRun) const
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				TCContinuation<t_CReturnValue> Continuation;
				try
				{
					Continuation.f_SetResult(_ToRun());
				}
				catch (t_CException const &)
				{
					Continuation.f_SetCurrentException();
				}
				return Continuation;
			}

			template <typename t_CReturnValue>
			template <typename tf_FToRun>
			TCContinuation<t_CReturnValue> TCRunProtectedHelper<t_CReturnValue, void>::operator > (tf_FToRun &&_ToRun) const
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				TCContinuation<t_CReturnValue> Continuation;
				try
				{
					Continuation.f_SetResult(_ToRun());
				}
				catch (...)
				{
					Continuation.f_SetCurrentException();
				}
				return Continuation;
			}

			template <typename t_CException>
			template <typename tf_FToRun>
			TCContinuation<void> TCRunProtectedHelper<void, t_CException>::operator > (tf_FToRun &&_ToRun) const
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				TCContinuation<void> Continuation;
				try
				{
					_ToRun();
					Continuation.f_SetResult();
				}
				catch (t_CException const &)
				{
					Continuation.f_SetCurrentException();
				}
				return Continuation;
			}

			template <typename tf_FToRun>
			TCContinuation<void> TCRunProtectedHelper<void, void>::operator > (tf_FToRun &&_ToRun) const
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				TCContinuation<void> Continuation;
				try
				{
					_ToRun();
					Continuation.f_SetResult();
				}
				catch (...)
				{
					Continuation.f_SetCurrentException();
				}
				return Continuation;
			}
		}
		
		template <typename t_CReturnValue>
		NPrivate::TCRunProtectedHelper<t_CReturnValue> TCContinuation<t_CReturnValue>::fs_RunProtected()
		{
			return NPrivate::TCRunProtectedHelper<t_CReturnValue>();
		}		

		template <typename t_CReturnValue>
		template <typename tf_CException>
		NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException> TCContinuation<t_CReturnValue>::fs_RunProtected()
		{
			return NPrivate::TCRunProtectedHelper<t_CReturnValue, tf_CException>();
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
			
			template <typename tf_FFunctor>
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
								typename NTraits::TCRemoveReference<tf_FFunctor>::CType
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
		auto TCContinuation<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
		{
			return [Continuation = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
				(auto &&...p_Results) mutable
				{
					bool bFailed = false;
					TCInitializerList<bool> Dummy = 
						{
							[&]
							{
								if (!bFailed && !p_Results)
								{
									Continuation.f_SetException(fg_Move(p_Results));
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
		auto TCContinuation<t_CReturnValue>::operator / (tf_FResultHandler &&_fResultHandler) const
		{
			return [Continuation = *this, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler)]
				(auto &&...p_Results) mutable
				{
					bool bFailed = false;
					TCInitializerList<bool> Dummy = 
						{
							[&]
							{
								if (!bFailed && !p_Results)
								{
									Continuation.f_SetException(fg_Move(p_Results));
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
		template <typename tf_CErrorString>
		auto TCContinuation<t_CReturnValue>::operator % (tf_CErrorString &&_ErrorString) const
		{
			return TCContinuationWithError<t_CReturnValue, typename NTraits::TCDecay<tf_CErrorString>::CType>(*this, _ErrorString);
		}
		
		template <typename t_CReturnValue, typename t_CError>
		TCContinuationWithError<t_CReturnValue, t_CError>::TCContinuationWithError(TCContinuation<t_CReturnValue> const &_Continuation, t_CError const &_Error)
			: m_Continuation(_Continuation)
			, m_Error(_Error)
		{
		}

		template <typename t_CReturnValue, typename t_CError>
		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
		auto TCContinuationWithError<t_CReturnValue, t_CError>::operator / (tf_FResultHandler &&_fResultHandler) const
		{
			return [Continuation = m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Error = m_Error]
				(auto &&...p_Results) mutable
				{
					bool bFailed = false;
					TCInitializerList<bool> Dummy = 
						{
							[&]
							{
								if (!bFailed && !p_Results)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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

		template <typename t_CReturnValue, typename t_CError>
		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> *>
		auto TCContinuationWithError<t_CReturnValue, t_CError>::operator / (tf_FResultHandler &&_fResultHandler) const
		{
			return [Continuation = m_Continuation, fResultHandler = fg_Forward<tf_FResultHandler>(_fResultHandler), Error = m_Error]
				(auto &&...p_Results) mutable
				{
					bool bFailed = false;
					TCInitializerList<bool> Dummy = 
						{
							[&]
							{
								if (!bFailed && !p_Results)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("{}: {}", Error, p_Results.f_GetExceptionStr())));
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
	}
}

