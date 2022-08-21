// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	void fg_Unwrap(TCAsyncResult<void> &&_ToUnwrap);
	void fg_UnwrapFirst(TCAsyncResult<void> &&_ToUnwrap);

	void fg_Unwrap
		(
		 	TCAsyncResult<void> &&_ToUnwrap
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	;

	template <typename tf_CData>
	tf_CData fg_Unwrap(TCAsyncResult<tf_CData> &&_ToUnwrap)
	{
		if (_ToUnwrap)
			return fg_Move(*_ToUnwrap);

		NException::CDisableExceptionTraceScope DisableExceptionTrace;
		DMibErrorCoroutineWrapper("Exception unwrapping async result", _ToUnwrap.f_GetException());
	}

	template <typename tf_CData>
	tf_CData fg_UnwrapFirst(TCAsyncResult<tf_CData> &&_ToUnwrap)
	{
		return fg_Unwrap(fg_Move(_ToUnwrap));
	}

	template <typename tf_CData>
	tf_CData fg_Unwrap
		(
		 	TCAsyncResult<tf_CData> &&_ToUnwrap
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		if (_ToUnwrap)
			return fg_Move(*_ToUnwrap);

		NException::CDisableExceptionTraceScope DisableExceptionTrace;
		DMibErrorCoroutineWrapper("Exception unwrapping async result", _fTransformer(_ToUnwrap.f_GetException()));
	}

	template <typename tf_CData, typename tf_CAllocator, typename tf_COptions>
	NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> fg_Unwrap(NContainer::TCVector<TCAsyncResult<tf_CData>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return.f_Insert(fg_Move(*Result));
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer());
		}

		return Return;
	}

	template <typename tf_CData, typename tf_CAllocator, typename tf_COptions>
	NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> fg_UnwrapFirst(NContainer::TCVector<TCAsyncResult<tf_CData>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NException::CExceptionPointer pException;

		NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return.f_Insert(fg_Move(*Result));
			else
			{
				if (!pException)
					pException = Result.f_GetException();
			}
		}

		if (pException)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", fg_Move(pException));
		}

		return Return;
	}

	template <typename tf_CData, typename tf_CAllocator, typename tf_COptions>
	NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> fg_Unwrap
		(
		 	NContainer::TCVector<TCAsyncResult<tf_CData>, tf_CAllocator, tf_COptions> &&_Results
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return.f_Insert(fg_Move(*Result));
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", _fTransformer(DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer()));
		}

		return Return;
	}

	template <typename tf_CAllocator, typename tf_COptions>
	void fg_Unwrap(NContainer::TCVector<TCAsyncResult<void>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		for (auto &Result : _Results)
		{
			if (!Result)
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer());
		}
	}

	template <typename tf_CAllocator, typename tf_COptions>
	void fg_UnwrapFirst(NContainer::TCVector<TCAsyncResult<void>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NException::CExceptionPointer pException;

		for (auto &Result : _Results)
		{
			if (!Result)
			{
				if (!pException)
					pException = Result.f_GetException();
			}
		}

		if (pException)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", fg_Move(pException));
		}
	}

	template <typename tf_CAllocator, typename tf_COptions>
	void fg_Unwrap
		(
		 	NContainer::TCVector<TCAsyncResult<void>, tf_CAllocator, tf_COptions> &&_Results
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		for (auto &Result : _Results)
		{
			if (!Result)
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result vector", _fTransformer(DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer()));
		}
	}

	template <typename tf_CKey, typename tf_CData, typename tf_CCompare, typename tf_CAllocator>
	auto fg_Unwrap(NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CData>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer());
		}

		return Return;
	}

	template <typename tf_CKey, typename tf_CData, typename tf_CCompare, typename tf_CAllocator>
	auto fg_UnwrapFirst(NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CData>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NException::CExceptionPointer pException;

		NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				if (!pException)
					pException = Result.f_GetException();
			}
		}

		if (pException)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", fg_Move(pException));
		}

		return Return;
	}

	template <typename tf_CKey, typename tf_CData, typename tf_CCompare, typename tf_CAllocator>
	NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator> fg_Unwrap
		(
		 	NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CData>, tf_CCompare, tf_CAllocator> &&_Results
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", _fTransformer(DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer()));
		}

		return Return;
	}

	template <typename tf_CKey, typename tf_CCompare, typename tf_CAllocator>
	NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> fg_Unwrap(NContainer::TCMap<tf_CKey, TCAsyncResult<void>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return[_Results.fs_GetKey(Result)];
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer());
		}

		return Return;
	}

	template <typename tf_CKey, typename tf_CCompare, typename tf_CAllocator>
	NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> fg_UnwrapFirst(NContainer::TCMap<tf_CKey, TCAsyncResult<void>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NException::CExceptionPointer pException;

		NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return[_Results.fs_GetKey(Result)];
			else
			{
				if (!pException)
					pException = Result.f_GetException();
			}
		}

		if (pException)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", fg_Move(pException));
		}

		return Return;
	}

	template <typename tf_CKey, typename tf_CCompare, typename tf_CAllocator>
	NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> fg_Unwrap
		(
		 	NContainer::TCMap<tf_CKey, TCAsyncResult<void>, tf_CCompare, tf_CAllocator> &&_Results
		 	, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)> const &_fTransformer
		)
	{
		NContainer::TCVector<NException::CExceptionPointer> Exceptions;
		NStr::CStr ErrorStr;
		bool bSuccess = true;

		NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return[_Results.fs_GetKey(Result)];
			else
			{
				Exceptions.f_Insert(Result.f_GetException());
				bSuccess = false;
				NStr::fg_AddStrSep(ErrorStr, Result.f_GetExceptionStr(), "\n");
			}
		}

		if (!bSuccess)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			DMibErrorCoroutineWrapper("Exception unwrapping async result map", _fTransformer(DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions)).f_ExceptionPointer()));
		}

		return Return;
	}
}

namespace NMib::NConcurrency::NUnwrap
{
	inline_always CUnwrapHelper::operator CUnwrapHelperWithTransformer const &()
	{
		return CUnwrapHelperWithTransformer::ms_EmptyTransformer;
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapHelper const &)
	{
		return fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap));
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapFirstHelper const &)
	{
		return fg_UnwrapFirst(fg_Forward<tf_CData>(_ToUnwrap));
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapHelperWithTransformer const &_Helper)
	{
		return fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap), _Helper.m_fTransformer);
	}

	CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer const &_Helper, NStr::CStr const &_Message);
}
