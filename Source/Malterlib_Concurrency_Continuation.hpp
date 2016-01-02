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

		template <typename t_CReturnValue>
		void TCContinuation<t_CReturnValue>::CData::fp_ReportNothingSet()
		{
			if ((m_OnResultSet.f_FetchOr(4) & (2 | 4)) == 2)
			{
				m_OnResult(); // Report nothing set
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
			if (m_OnResultSet.f_FetchOr(1) & 2)
			{
				m_OnResult();
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
	}
}
