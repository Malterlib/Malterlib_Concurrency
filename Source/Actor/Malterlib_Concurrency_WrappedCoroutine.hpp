// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CType>
	bool TCWrappedSyncAwaiter<t_CType>::await_ready() noexcept
	{
		return bool(m_Result);
	}

	template <typename t_CType>
	template <typename tf_CPromise>
	void TCWrappedSyncAwaiter<t_CType>::await_suspend(TCCoroutineHandle<tf_CPromise> _Handle) noexcept
	{
		_Handle.promise().m_Result.f_SetException(fg_Move(m_Result).f_GetException());
	}

	template <typename t_CType>
	auto TCWrappedSyncAwaiter<t_CType>::await_resume() noexcept
	{
		return fg_Move(*m_Result);
	}

	template <typename t_CType>
	mark_artificial inline_always TCWrappedCoroutineReturnObject<t_CType>::TCWrappedCoroutineReturnObject(CHandle _Handle) noexcept
		: m_Handle(_Handle)
	{
	}

	template <typename t_CType>
	mark_artificial inline_always TCWrappedCoroutineReturnObject<t_CType>::TCWrappedCoroutineReturnObject(TCWrappedCoroutineReturnObject &&_Other) noexcept
		: m_Handle(_Other.m_Handle)
	{
		_Other.m_Handle = nullptr;
	}

	template <typename t_CType>
	mark_artificial inline_always TCWrappedCoroutineReturnObject<t_CType>::~TCWrappedCoroutineReturnObject()
	{
		if (m_Handle)
			m_Handle.destroy();
	}

	template <typename t_CType>
	mark_artificial inline_always TCWrappedCoroutineReturnObject<t_CType>::operator TCWrapped<t_CType>()
	{
		auto Handle = m_Handle;
		m_Handle = nullptr;
		Handle.resume();
		TCWrapped<t_CType> Result;
		static_cast<TCAsyncResult<t_CType> &>(Result) = fg_Move(Handle.promise().m_Result);
		Handle.destroy();
		return Result;
	}

	template <typename t_CType>
	TCWrappedCoroutineReturnObject<t_CType> TCWrappedCoroutinePromiseBase<t_CType>::get_return_object() noexcept
	{
		return TCWrappedCoroutineReturnObject<t_CType>
			(
				TCCoroutineHandle<TCWrappedCoroutinePromise<t_CType>>::from_promise
					(
						static_cast<TCWrappedCoroutinePromise<t_CType> &>(*this)
					)
			)
		;
	}

	template <typename t_CType>
	CSuspendAlways TCWrappedCoroutinePromiseBase<t_CType>::initial_suspend() noexcept
	{
		return {};
	}

	template <typename t_CType>
	CSuspendAlways TCWrappedCoroutinePromiseBase<t_CType>::final_suspend() noexcept
	{
		return {};
	}

	template <typename t_CType>
	void TCWrappedCoroutinePromiseBase<t_CType>::unhandled_exception() noexcept
	{
		m_Result.f_SetCurrentException();
	}

	template <typename t_CType>
	template <typename tf_CType>
	TCWrappedSyncAwaiter<tf_CType> TCWrappedCoroutinePromiseBase<t_CType>::await_transform(TCAsyncResult<tf_CType> &&_Result) noexcept
	{
		return {fg_Move(_Result)};
	}

	template <typename t_CType>
	template <typename tf_CType>
	TCWrappedSyncAwaiter<tf_CType> TCWrappedCoroutinePromiseBase<t_CType>::await_transform(TCWrapped<tf_CType> &&_Result) noexcept
	{
		return {static_cast<TCAsyncResult<tf_CType> &&>(_Result)};
	}

	template <typename t_CType>
	void TCWrappedCoroutinePromise<t_CType>::return_value(t_CType &&_Value)
	{
		this->m_Result.f_SetResult(fg_Move(_Value));
	}

	template <typename t_CType>
	template <typename tf_CReturnType>
	void TCWrappedCoroutinePromise<t_CType>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>;
		if constexpr (NTraits::cIsSame<CReturnNoReference, TCAsyncResult<t_CType>>)
			this->m_Result = fg_Forward<tf_CReturnType>(_Value);
		else if constexpr
			(
				NPrivate::TCIsAsyncResult<CReturnNoReference>::mc_Value
				&& NTraits::cIsConstructibleWith<TCWrapped<t_CType>, tf_CReturnType &&>
			)
		{
			TCWrapped<t_CType> Converted(fg_Forward<tf_CReturnType>(_Value));
			this->m_Result = static_cast<TCAsyncResult<t_CType> &&>(Converted);
		}
		else if constexpr (NTraits::cIsSame<CReturnNoReference, TCWrapped<t_CType>>)
			this->m_Result = fg_ForwardAs<tf_CReturnType>(static_cast<TCAsyncResult<t_CType> &>(_Value));
		else if constexpr
			(
				NPrivate::TCIsWrapped<CReturnNoReference>::mc_Value
				&& NTraits::cIsConstructibleWith<TCWrapped<t_CType>, tf_CReturnType &&>
			)
		{
			TCWrapped<t_CType> Converted(fg_Forward<tf_CReturnType>(_Value));
			this->m_Result = static_cast<TCAsyncResult<t_CType> &&>(Converted);
		}
		else if constexpr (NException::cIsException<CReturnNoReference>)
		{
			static_assert
				(
					(NTraits::cIsRValueReference<tf_CReturnType> || !NTraits::cIsReference<tf_CReturnType>)
					&& !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CReturnType>>
					, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			this->m_Result.f_SetException(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::cIsSame<CReturnNoReference, NException::CExceptionPointer>)
			this->m_Result.f_SetException(fg_Forward<tf_CReturnType>(_Value));
		else
			this->m_Result.f_SetResult(fg_Forward<tf_CReturnType>(_Value));
	}

	template <typename tf_CReturnType>
	void TCWrappedCoroutinePromise<void>::return_value(tf_CReturnType &&_Value)
	{
		using CReturnNoReference = NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>;
		static_assert
			(
				NTraits::cIsSame<CReturnNoReference, TCAsyncResult<void>>
				|| NTraits::cIsSame<CReturnNoReference, TCWrapped<void>>
				|| NException::cIsException<CReturnNoReference>
				|| NTraits::cIsSame<CReturnNoReference, NException::CExceptionPointer>
				, "You can only return exceptions or async results"
			)
		;

		if constexpr (NTraits::cIsSame<CReturnNoReference, TCAsyncResult<void>>)
			this->m_Result = fg_Forward<tf_CReturnType>(_Value);
		else if constexpr (NTraits::cIsSame<CReturnNoReference, TCWrapped<void>>)
			this->m_Result = fg_ForwardAs<tf_CReturnType>(static_cast<TCAsyncResult<void> &>(_Value));
		else
		{
			if constexpr (NException::cIsException<CReturnNoReference>)
			{
				static_assert
					(
						(NTraits::cIsRValueReference<tf_CReturnType> || !NTraits::cIsReference<tf_CReturnType>)
						&& !NTraits::cIsConst<NTraits::TCRemoveReference<tf_CReturnType>>
						, "Only safe to return newly created exceptions, otherwise use exception pointers"
					)
				;
				this->m_Result.f_SetException(fg_Forward<tf_CReturnType>(_Value));
			}
			else
				this->m_Result.f_SetException(fg_Forward<tf_CReturnType>(_Value));
		}
	}

	inline void TCWrappedCoroutinePromise<void>::return_value(CVoidTag &&)
	{
		this->m_Result.f_SetResult();
	}

	inline void TCWrappedCoroutinePromise<void>::return_value(CVoidTag const &)
	{
		this->m_Result.f_SetResult();
	}
}
