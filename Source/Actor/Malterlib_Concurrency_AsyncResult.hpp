// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always CAsyncResult::EDataType CAsyncResult::CDataUnion::f_DataType() const
	{
		return CAsyncResult::EDataType(m_DataType & 0x3);
	}

	template <typename tf_CException, TCEnableIfType<NTraits::TCIsBaseOf<typename NTraits::TCRemoveReferenceAndQualifiers<tf_CException>::CType, NException::CExceptionBase>::mc_Value> *>
	void CAsyncResult::f_SetException(tf_CException &&_Exception)
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		f_SetException(NException::fg_ExceptionPointer(fg_Forward<tf_CException>(_Exception)));
	}

	template <typename tf_CException>
	bool CAsyncResult::f_HasExceptionType() const
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
			return NException::fg_ExceptionIsOfType<tf_CException>(mp_Data.m_pException);
		else if (mp_Data.f_DataType() == EDataType_None)
			return NTraits::TCIsBaseOfOrSame<NException::CException, tf_CException>::mc_Value;
		return false;
	}
	
	template <typename t_CType>
	TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult const &_Other)
		: CAsyncResult(_Other)
	{
		if (_Other.mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Construct(*_Other.m_ResultAggregate);
	}
	
	template <typename t_CType>
	TCAsyncResult<t_CType>::TCAsyncResult(TCAsyncResult &&_Other)
		: CAsyncResult(fg_Move(_Other))
	{
		if (_Other.mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Construct(fg_Move(*_Other.m_ResultAggregate));
	}
	
	template <typename t_CType>
	auto TCAsyncResult<t_CType>::operator = (TCAsyncResult const &_Other) -> TCAsyncResult &
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Destruct();
		CAsyncResult::operator = (_Other);
		if (_Other.mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Construct(*_Other.m_ResultAggregate);
		return *this;
	}
	
	template <typename t_CType>
	auto TCAsyncResult<t_CType>::operator =(TCAsyncResult &&_Other) -> TCAsyncResult &
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Destruct();
		CAsyncResult::operator = (fg_Move(_Other));
		if (_Other.mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Construct(fg_Move(*_Other.m_ResultAggregate));
		return *this;
	}

	template <typename t_CType>
	TCAsyncResult<t_CType>::~TCAsyncResult()
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Destruct();
	}

	template <typename t_CType>
	void TCAsyncResult<t_CType>::f_Clear()
	{
		using namespace NException;

		if (mp_Data.f_DataType() == EDataType_HasBeenSet)
			m_ResultAggregate.f_Destruct();
		else if (mp_Data.f_DataType() == EDataType_Exception)
			mp_Data.m_pException.~CExceptionPointer();

		mp_Data.m_DataType = EDataType_None;
	}
	
	template <typename t_CType>
	inline_always t_CType const &TCAsyncResult<t_CType>::f_Get() const
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return *m_ResultAggregate;

		fp_AccessSlowPath();
	}

	template <typename t_CType>
	inline_always t_CType &TCAsyncResult<t_CType>::f_Get()
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return *m_ResultAggregate;

		fp_AccessSlowPath();
	}

	template <typename t_CType>
	inline_always t_CType TCAsyncResult<t_CType>::f_Move()
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return fg_Move(*m_ResultAggregate);

		fp_AccessSlowPath();
	}

	template <typename t_CType>
	mark_nodebug inline_always t_CType const &TCAsyncResult<t_CType>::operator *() const
	{
		return f_Get();;
	}
	template <typename t_CType>
	mark_nodebug inline_always t_CType &TCAsyncResult<t_CType>::operator *()
	{
		return f_Get();;
	}

	template <typename t_CType>
	mark_nodebug inline_always t_CType const *TCAsyncResult<t_CType>::operator ->() const
	{
		return &f_Get();;
	}
	template <typename t_CType>
	mark_nodebug inline_always t_CType *TCAsyncResult<t_CType>::operator ->()
	{
		return &f_Get();;
	}

	template <typename t_CType>
	void TCAsyncResult<t_CType>::f_SetResult(TCAsyncResult const &_Result)
	{
		*this = _Result;
	}

	template <typename t_CType>
	void TCAsyncResult<t_CType>::f_SetResult(TCAsyncResult &_Result)
	{
		*this = _Result;
	}

	template <typename t_CType>
	void TCAsyncResult<t_CType>::f_SetResult(TCAsyncResult &&_Result)
	{
		*this = fg_Move(_Result);
	}

	template <typename t_CType>
	template <typename ...tfp_CType>
	void TCAsyncResult<t_CType>::f_SetResult(tfp_CType && ...p_Result)
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		m_ResultAggregate.f_Construct(fg_Forward<tfp_CType>(p_Result)...);
		mp_Data.m_DataType = EDataType_HasBeenSet;
	}

	template <typename t_CType>
	t_CType &TCAsyncResult<t_CType>::f_PrepareResult()
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		m_ResultAggregate.f_Construct();
		mp_Data.m_DataType = EDataType_HasBeenSet;

		return *m_ResultAggregate;
	}

	template <typename t_CType>
	template <typename tf_CStr>
	void TCAsyncResult<t_CType>::f_Format(tf_CStr &o_Str) const
	{
		if (*this)
			o_Str += typename tf_CStr::CFormat("{}") << **this;
		else
			o_Str += typename tf_CStr::CFormat("[{}]") << f_GetExceptionStr();
	}

	template <typename tf_CStr>
	void TCAsyncResult<void>::f_Format(tf_CStr &o_Str) const
	{
		if (*this)
			o_Str += typename tf_CStr::CFormat("{}") << **this;
		else
			o_Str += typename tf_CStr::CFormat("[{}]") << f_GetExceptionStr();
	}
}
