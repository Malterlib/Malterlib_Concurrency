// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	TCAsyncResult<void> fg_Unwrap(TCAsyncResult<void> &&_ToUnwrap);
	TCAsyncResult<void> fg_UnwrapFirst(TCAsyncResult<void> &&_ToUnwrap);

	template <typename tf_CData>
	TCAsyncResult<tf_CData> fg_Unwrap(TCAsyncResult<tf_CData> &&_ToUnwrap)
	{
		return fg_Move(_ToUnwrap);
	}

	template <typename tf_CData>
	TCAsyncResult<tf_CData> fg_UnwrapFirst(TCAsyncResult<tf_CData> &&_ToUnwrap)
	{
		return fg_Move(_ToUnwrap);
	}

	template <typename tf_CData, typename tf_CAllocator, typename tf_COptions>
	TCAsyncResult<NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions>> fg_Unwrap(NContainer::TCVector<TCAsyncResult<tf_CData>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return.f_Insert(fg_Move(*Result));
			else
			{
				bSuccess = false;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}

		TCAsyncResult<NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions>> ReturnResult;
		if (!bSuccess)
			ReturnResult.f_SetException(fg_Move(ErrorCollector).f_GetException());
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}

	template <typename tf_CData, typename tf_CAllocator, typename tf_COptions>
	TCAsyncResult<NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions>> fg_UnwrapFirst(NContainer::TCVector<TCAsyncResult<tf_CData>, tf_CAllocator, tf_COptions> &&_Results)
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

		TCAsyncResult<NContainer::TCVector<tf_CData, tf_CAllocator, tf_COptions>> ReturnResult;

		if (pException)
			ReturnResult.f_SetException(fg_Move(pException));
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}

	template <typename tf_CAllocator, typename tf_COptions>
	TCAsyncResult<void> fg_Unwrap(NContainer::TCVector<TCAsyncResult<void>, tf_CAllocator, tf_COptions> &&_Results)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		for (auto &Result : _Results)
		{
			if (!Result)
			{
				bSuccess = false;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}

		TCAsyncResult<void> ReturnResult;

		if (!bSuccess)
			ReturnResult.f_SetException(fg_Move(ErrorCollector).f_GetException());
		else
			ReturnResult.f_SetResult();

		return ReturnResult;
	}

	template <typename tf_CAllocator, typename tf_COptions>
	TCAsyncResult<void> fg_UnwrapFirst(NContainer::TCVector<TCAsyncResult<void>, tf_CAllocator, tf_COptions> &&_Results)
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

		TCAsyncResult<void> ReturnResult;
		if (pException)
			ReturnResult.f_SetException(fg_Move(pException));
		else
			ReturnResult.f_SetResult();

		return ReturnResult;
	}

	template <typename tf_CKey, typename tf_CData, typename tf_CCompare, typename tf_CAllocator>
	TCAsyncResult<NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator>> fg_Unwrap(NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CData>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bSuccess = false;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}

		TCAsyncResult<NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator>> ReturnResult;

		if (!bSuccess)
			ReturnResult.f_SetException(fg_Move(ErrorCollector).f_GetException());
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}

	template <typename tf_CKey, typename tf_CData, typename tf_CCompare, typename tf_CAllocator>
	TCAsyncResult<NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator>> fg_UnwrapFirst(NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CData>, tf_CCompare, tf_CAllocator> &&_Results)
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

		TCAsyncResult<NContainer::TCMap<tf_CKey, tf_CData, tf_CCompare, tf_CAllocator>> ReturnResult;

		if (pException)
			ReturnResult.f_SetException(fg_Move(pException));
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}

	template <typename tf_CKey, typename tf_CCompare, typename tf_CAllocator>
	TCAsyncResult<NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator>> fg_Unwrap(NContainer::TCMap<tf_CKey, TCAsyncResult<void>, tf_CCompare, tf_CAllocator> &&_Results)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bSuccess = true;

		NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator> Return;

		for (auto &Result : _Results)
		{
			if (Result)
				Return[_Results.fs_GetKey(Result)];
			else
			{
				bSuccess = false;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}

		TCAsyncResult<NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator>> ReturnResult;

		if (!bSuccess)
			ReturnResult.f_SetException(fg_Move(ErrorCollector).f_GetException());
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}

	template <typename tf_CKey, typename tf_CCompare, typename tf_CAllocator>
	TCAsyncResult<NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator>> fg_UnwrapFirst(NContainer::TCMap<tf_CKey, TCAsyncResult<void>, tf_CCompare, tf_CAllocator> &&_Results)
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

		TCAsyncResult<NContainer::TCSet<tf_CKey, tf_CCompare, tf_CAllocator>> ReturnResult;

		if (pException)
			ReturnResult.f_SetException(fg_Move(pException));
		else
			ReturnResult.f_SetResult(fg_Move(Return));

		return ReturnResult;
	}
}

namespace NMib::NConcurrency::NUnwrap
{
	template <typename t_CReturnType, typename t_FExceptionTransform = CVoidTag>
	struct [[nodiscard]] TCUnwrapAwaiter;

	template <typename t_CReturnType, typename t_FExceptionTransform>
	struct [[nodiscard]] TCUnwrapAwaiter<TCAsyncResult<t_CReturnType>, t_FExceptionTransform>
	{
		TCUnwrapAwaiter(TCAsyncResult<t_CReturnType> &&_Result, t_FExceptionTransform &&_fExceptionTransform = {})
			: mp_Result(fg_Move(_Result))
			, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
		{
		}

		bool await_ready() noexcept
		{
			if (mp_Result)
				return true;
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle) noexcept
		{
			auto &CoroutineContext = _Handle.promise();

			DMibFastCheck(!mp_Result);

			CoroutineContext.f_HandleAwaitedException
				(
					fg_ConcurrencyThreadLocal()
					, &tf_CCoroutineContext::fs_KeepaliveSetExceptionFunctor
					, fg_TransformException(fg_Move(mp_Result).f_GetException(), mp_fExceptionTransform)
				)
			;
			return true;
		}

		auto await_resume() noexcept
		{
			return fg_Move(*mp_Result);
		}

	private:
		TCAsyncResult<t_CReturnType> mp_Result;
		DMibNoUniqueAddress t_FExceptionTransform mp_fExceptionTransform;
	};


	inline_always CUnwrapHelper::operator CUnwrapHelperWithTransformer const &()
	{
		return CUnwrapHelperWithTransformer::ms_EmptyTransformer;
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapHelper const &)
	{
		using CResultType = decltype(fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap)));

		return TCUnwrapAwaiter<CResultType>(fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap)));
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapFirstHelper const &)
	{
		using CResultType = decltype(fg_UnwrapFirst(fg_Forward<tf_CData>(_ToUnwrap)));

		return TCUnwrapAwaiter<CResultType>(fg_UnwrapFirst(fg_Forward<tf_CData>(_ToUnwrap)));
	}

	template <typename tf_CData>
	decltype(auto) operator | (tf_CData &&_ToUnwrap, CUnwrapHelperWithTransformer &&_Helper)
	{
		using CResultType = decltype(fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap)));

		return TCUnwrapAwaiter<CResultType, decltype(_Helper.m_fTransformer)>(fg_Unwrap(fg_Forward<tf_CData>(_ToUnwrap)), fg_Move(_Helper.m_fTransformer));
	}

	template <typename tf_CTransformerParam>
	CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer const &_Helper, tf_CTransformerParam &&_Param)
	{
		return CUnwrapHelperWithTransformer(fg_ExceptionTransformer(fg_TempCopy(_Helper.m_fTransformer), fg_Forward<tf_CTransformerParam>(_Param)));
	}

	template <typename tf_CTransformerParam>
	CUnwrapHelperWithTransformer operator % (CUnwrapHelperWithTransformer &&_Helper, tf_CTransformerParam &&_Param)
	{
		return CUnwrapHelperWithTransformer(fg_ExceptionTransformer(fg_Move(_Helper.m_fTransformer), fg_Forward<tf_CTransformerParam>(_Param)));
	}
}
