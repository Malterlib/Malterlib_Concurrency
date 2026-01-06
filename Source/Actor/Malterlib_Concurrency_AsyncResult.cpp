// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <exception>
#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_AsyncResult.h"

namespace NMib::NConcurrency
{
	inline_always_lto CAsyncResult::CDataUnion::CDataUnion()
		: m_DataType(EDataType_None)
	{
	}

	inline_always_lto CAsyncResult::CDataUnion::CDataUnion(CDataUnion const &_Other)
	{
		using namespace NException;

		if (_Other.f_DataType() == EDataType_Exception)
			new (&m_pException) CExceptionPointer(_Other.m_pException);
		else
			m_DataType = _Other.m_DataType;
	}

	inline_always_lto CAsyncResult::CDataUnion::CDataUnion(CDataUnion &&_Other)
	{
		using namespace NException;

		if (_Other.f_DataType() == EDataType_Exception)
			new (&m_pException) CExceptionPointer(fg_Move(_Other.m_pException));
		else
			m_DataType = _Other.m_DataType;
	}

	inline_always_lto auto CAsyncResult::CDataUnion::operator = (CDataUnion const &_Other) -> CDataUnion &
	{
		using namespace NException;

		if (f_DataType() == EDataType_Exception)
			m_pException.~CExceptionPointer();

		if (_Other.f_DataType() == EDataType_Exception)
			new (&m_pException) CExceptionPointer(_Other.m_pException);
		else
			m_DataType = _Other.m_DataType;

		return *this;
	}

	inline_always_lto auto CAsyncResult::CDataUnion::operator = (CDataUnion &&_Other) -> CDataUnion &
	{
		using namespace NException;

		if (f_DataType() == EDataType_Exception)
			m_pException.~CExceptionPointer();

		if (_Other.f_DataType() == EDataType_Exception)
			new (&m_pException) CExceptionPointer(fg_Move(_Other.m_pException));
		else
			m_DataType = _Other.m_DataType;

		return *this;
	}

	inline_always_lto CAsyncResult::CDataUnion::~CDataUnion()
	{
		using namespace NException;
		if (f_DataType() == EDataType_Exception)
			m_pException.~CExceptionPointer();
	}

	CAsyncResult::CAsyncResult(CAsyncResult const &_Other) = default;
	CAsyncResult::CAsyncResult(CAsyncResult &&_Other) = default;
	CAsyncResult::CAsyncResult() = default;
	CAsyncResult &CAsyncResult::operator =(CAsyncResult const &_Other) = default;
	CAsyncResult &CAsyncResult::operator =(CAsyncResult &&_Other) = default;

	NException::CExceptionPointer const &CAsyncResult::f_ExceptionPointer() const
	{
		DMibFastCheck(mp_Data.f_DataType() == EDataType_Exception);
		return mp_Data.m_pException;
	}

	void CAsyncResult::f_Access() const
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
			std::rethrow_exception(mp_Data.m_pException);
		else if (mp_Data.f_DataType() == EDataType_None)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorActorResultWasNotSet("No result specified");
		}
	}

	[[noreturn]] inline_never void CAsyncResult::fp_AccessSlowPath() const
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
			std::rethrow_exception(mp_Data.m_pException);
		else if (mp_Data.f_DataType() == EDataType_None)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorActorResultWasNotSet("No result specified");
		}
		else
			DMibErrorActorResultWasNotSet("Internal error");
	}

	namespace
	{
		NException::CExceptionPointer const &fg_NoResultException() noexcept
		{
			CConcurrencyThreadLocal &ThreadLocal = fg_ConcurrencyThreadLocal();
			if (!ThreadLocal.m_pNoResultException)
				ThreadLocal.m_pNoResultException = fg_MakeException(DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "No result specified", false));
			return ThreadLocal.m_pNoResultException;
		}
	}

	NException::CExceptionPointer const &CAsyncResult::fs_ResultWasNotSetException()
	{
		CConcurrencyThreadLocal &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (!ThreadLocal.m_pResultWasNotSetException)
			ThreadLocal.m_pResultWasNotSetException = fg_MakeException(DMibImpExceptionInstance(CExceptionActorResultWasNotSet, "Result was not set", false));
		return ThreadLocal.m_pResultWasNotSetException;
	}

	NException::CExceptionPointer const &CAsyncResult::fs_ActorCalledDeletedException()
	{
		CConcurrencyThreadLocal &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (!ThreadLocal.m_pActorCalledDeletedException)
			ThreadLocal.m_pActorCalledDeletedException = fg_MakeException(DMibImpExceptionInstance(CExceptionActorDeleted, "Actor called has been deleted", false));
		return ThreadLocal.m_pActorCalledDeletedException;
	}

	NException::CExceptionPointer const &CAsyncResult::fs_ActorAlreadyDestroyedException()
	{
		CConcurrencyThreadLocal &ThreadLocal = fg_ConcurrencyThreadLocal();
		if (!ThreadLocal.m_pActorAlreadyDestroyedException)
			ThreadLocal.m_pActorAlreadyDestroyedException = fg_MakeException(DMibImpExceptionInstance(CExceptionActorAlreadyDestroyed, "Actor destruction already in progress", false));
		return ThreadLocal.m_pActorAlreadyDestroyedException;
	}

	NException::CExceptionPointer CAsyncResult::f_GetException() const & noexcept
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
			return mp_Data.m_pException;
		else if (mp_Data.f_DataType() == EDataType_None)
			return fg_NoResultException();
		return nullptr;
	}

	NException::CExceptionPointer CAsyncResult::f_GetException() && noexcept
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
			return fg_Move(mp_Data.m_pException);
		else if (mp_Data.f_DataType() == EDataType_None)
			return fg_NoResultException();
		return nullptr;
	}

	NStr::CStr CAsyncResult::f_GetExceptionStr() const noexcept
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
		{
			NStr::CStr Return;
			if
				(
					!NException::fg_VisitException<NException::CExceptionBase>
					(
						mp_Data.m_pException
						, [&](NException::CExceptionBase const& _Exception)
						{
							Return = _Exception.f_GetErrorStr();
						}
					)
				)
			{
				return NStr::gc_Str<"Unknown exception type">;
			}

			return Return;
		}
		else if (mp_Data.f_DataType() == EDataType_None)
			return NStr::gc_Str<"No result specified">;

		return NStr::gc_Str<"No error (no exception was set)">;
	}

	NStr::CStr CAsyncResult::f_GetExceptionCallstackStr(mint _Indent) const
	{
		if (mp_Data.f_DataType() == EDataType_Exception)
		{
			NStr::CStr Return;
			NException::fg_VisitException<NException::CExceptionBase>
				(
					mp_Data.m_pException
					, [&](NException::CExceptionBase const& _Exception)
					{
						Return = _Exception.f_GetCallstackStr(_Indent);
					}
				)
			;

			return Return;
		}

		return {};
	}

	TCAsyncResult<void>::TCAsyncResult() = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void>::TCAsyncResult(TCAsyncResult const &_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult &&_Other) = default;
	TCAsyncResult<void> &TCAsyncResult<void>::operator =(TCAsyncResult const &_Other) = default;

	TCAsyncResult<void>::~TCAsyncResult() = default;

	void TCAsyncResult<void>::f_Clear()
	{
		using namespace NException;

		if (mp_Data.f_DataType() == EDataType_HasBeenSet)
			;
		else if (mp_Data.f_DataType() == EDataType_Exception)
			mp_Data.m_pException.~CExceptionPointer();

		mp_Data.m_DataType = EDataType_None;
	}

	inline_always_lto CVoidTag TCAsyncResult<void>::f_Get() const
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return CVoidTag();

		fp_AccessSlowPath();
	}

	inline_always_lto CVoidTag TCAsyncResult<void>::f_Get()
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return CVoidTag();

		fp_AccessSlowPath();
	}

	inline_always_lto CVoidTag TCAsyncResult<void>::f_Move()
	{
		if (mp_Data.f_DataType() == EDataType_HasBeenSet) [[likely]]
			return CVoidTag();

		fp_AccessSlowPath();
	}

	mark_nodebug inline_always_lto CVoidTag TCAsyncResult<void>::operator *() const
	{
		return f_Get();
	}

	mark_nodebug inline_always_lto CVoidTag TCAsyncResult<void>::operator *()
	{
		return f_Get();
	}

	void TCAsyncResult<void>::f_SetResult(TCAsyncResult const &_Result)
	{
		*this = _Result;
	}

	void TCAsyncResult<void>::f_SetResult(TCAsyncResult &_Result)
	{
		*this = _Result;
	}

	void TCAsyncResult<void>::f_SetResult(TCAsyncResult &&_Result)
	{
		*this = fg_Move(_Result);
	}

	void CAsyncResult::f_SetException(NException::CExceptionPointer const &_pException)
	{
		using namespace NException;

		DMibRequire(mp_Data.f_DataType() == EDataType_None || mp_Data.f_DataType() == EDataType_Exception);

		if (mp_Data.f_DataType() == EDataType_Exception)
			mp_Data.m_pException.~CExceptionPointer();

		new (&mp_Data.m_pException) CExceptionPointer(_pException);
	}

	void CAsyncResult::f_SetException(NException::CExceptionPointer &&_pException)
	{
		using namespace NException;

		DMibRequire(mp_Data.f_DataType() == EDataType_None || mp_Data.f_DataType() == EDataType_Exception);

		if (mp_Data.f_DataType() == EDataType_Exception)
			mp_Data.m_pException.~CExceptionPointer();

		new (&mp_Data.m_pException) CExceptionPointer(fg_Move(_pException));
	}

	void CAsyncResult::f_SetExceptionAppendable(NException::CExceptionPointer &&_pException)
	{
		if (f_IsSet())
		{
			if (*this)
				*this = {};
			else
			{
				NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
				ErrorCollector.f_AddError(fg_Move(*this).f_GetException());
				ErrorCollector.f_AddError(fg_Move(_pException));

				*this = {};
				f_SetException(fg_Move(ErrorCollector).f_GetException());

				return;
			}
		}

		f_SetException(_pException);
	}

	void CAsyncResult::f_SetException(CAsyncResult &&_AsyncResult)
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		DMibRequire(_AsyncResult.mp_Data.f_DataType() == EDataType_Exception);
		f_SetException(fg_Move(_AsyncResult.mp_Data.m_pException));
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_Callstacks = fg_Move(_AsyncResult.m_Callstacks);
#endif
	}

	void CAsyncResult::f_SetException(CAsyncResult const &_AsyncResult)
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		DMibRequire(_AsyncResult.mp_Data.f_DataType() == EDataType_Exception);
		f_SetException(_AsyncResult.mp_Data.m_pException);
#if DMibConfig_Concurrency_DebugActorCallstacks
		m_Callstacks = _AsyncResult.m_Callstacks;
#endif
	}

	void CAsyncResult::f_SetCurrentException()
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		f_SetException(NException::fg_CurrentException());
	}

	void TCAsyncResult<void>::f_SetResult()
	{
		DMibRequire(mp_Data.f_DataType() == EDataType_None);
		mp_Data.m_DataType = EDataType_HasBeenSet;
	}

	CAsyncResult::operator bool () const
	{
		return mp_Data.f_DataType() == EDataType_HasBeenSet;
	}

	bool CAsyncResult::f_IsSet() const
	{
		return mp_Data.f_DataType() != EDataType_None;
	}
}

namespace NMib::NFunction
{
	template struct TCFunctionMovable<void (NConcurrency::TCAsyncResult<void> &&)>;
}
