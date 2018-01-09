// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CException, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CException>::CType, NException::CException>::mc_Value> *>
	void CAsyncResult::f_SetException(tf_CException &&_Exception)
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_pException = NException::fg_ExceptionPointer(fg_Forward<tf_CException>(_Exception));
	}

	
	template <typename t_CType>
	TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult const &_Other)
		: CAsyncResult(_Other)
	{
		if (_Other.m_bHasBeenSet)
			m_ResultAggregate.f_Construct(*_Other.m_ResultAggregate);
	}
	
	template <typename t_CType>
	TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult &&_Other)
		: CAsyncResult(fg_Move(_Other))
	{
		if (_Other.m_bHasBeenSet)
			m_ResultAggregate.f_Construct(fg_Move(*_Other.m_ResultAggregate));
	}
	
	template <typename t_CType>
	auto TCAsyncResult<t_CType>::operator = (TCAsyncResult const &_Other) -> TCAsyncResult &
	{
		if (m_bHasBeenSet)
			m_ResultAggregate.f_Destruct();
		CAsyncResult::operator = (_Other);
		if (_Other.m_bHasBeenSet)
			m_ResultAggregate.f_Construct(*_Other.m_ResultAggregate);
		return *this;
	}
	
	template <typename t_CType>
	auto TCAsyncResult<t_CType>::operator =(TCAsyncResult &&_Other) -> TCAsyncResult &
	{
		if (m_bHasBeenSet)
			m_ResultAggregate.f_Destruct();
		CAsyncResult::operator = (fg_Move(_Other));
		if (_Other.m_bHasBeenSet)
			m_ResultAggregate.f_Construct(fg_Move(*_Other.m_ResultAggregate));
		return *this;
	}
	
	template <typename t_CType>
	TCAsyncResult<t_CType>::~TCAsyncResult()
	{
		if (m_bHasBeenSet)
			m_ResultAggregate.f_Destruct();
	}
	
	template <typename t_CType>
	t_CType const &TCAsyncResult<t_CType>::f_Get() const
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
			DMibError("No result specified");

		return *m_ResultAggregate;
	}
	template <typename t_CType>
	t_CType &TCAsyncResult<t_CType>::f_Get()
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
			DMibError("No result specified");

		return *m_ResultAggregate;
	}
	template <typename t_CType>
	t_CType TCAsyncResult<t_CType>::f_Move()
	{
		if (m_pException != nullptr)
			std::rethrow_exception(m_pException);
		else if (!m_bHasBeenSet)
			DMibError("No result specified");

		return fg_Move(*m_ResultAggregate);
	}

	template <typename t_CType>
	t_CType const &TCAsyncResult<t_CType>::operator *() const
	{
		return f_Get();;
	}
	template <typename t_CType>
	t_CType &TCAsyncResult<t_CType>::operator *()
	{
		return f_Get();;
	}

	template <typename t_CType>
	t_CType const *TCAsyncResult<t_CType>::operator ->() const
	{
		return &f_Get();;
	}
	template <typename t_CType>
	t_CType *TCAsyncResult<t_CType>::operator ->()
	{
		return &f_Get();;
	}

	template <typename t_CType>
	template <typename ...tfp_CType>
	void TCAsyncResult<t_CType>::f_SetResult(tfp_CType && ...p_Result)
	{
		DMibRequire(!m_bHasBeenSet);
		DMibRequire(!m_pException);
		m_ResultAggregate.f_Construct(fg_Forward<tfp_CType>(p_Result)...);
		m_bHasBeenSet = true;
	}
}
