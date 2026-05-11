// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "Malterlib_Concurrency_Coroutine.h"

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CType>
	struct TCWrappedCoroutinePromise;

	template <typename t_CType>
	struct TCWrappedSyncAwaiter
	{
		TCAsyncResult<t_CType> m_Result;

		bool await_ready() noexcept;

		template <typename tf_CPromise>
		void await_suspend(TCCoroutineHandle<tf_CPromise> _Handle) noexcept;

		auto await_resume() noexcept;
	};

	template <typename t_CType>
	struct [[nodiscard]] TCWrappedCoroutineReturnObject
	{
		using CHandle = TCCoroutineHandle<TCWrappedCoroutinePromise<t_CType>>;

		mark_artificial inline_always TCWrappedCoroutineReturnObject(CHandle _Handle) noexcept;

		TCWrappedCoroutineReturnObject(TCWrappedCoroutineReturnObject const &) = delete;
		TCWrappedCoroutineReturnObject &operator =(TCWrappedCoroutineReturnObject const &) = delete;
		TCWrappedCoroutineReturnObject &operator =(TCWrappedCoroutineReturnObject &&) = delete;

		mark_artificial inline_always TCWrappedCoroutineReturnObject(TCWrappedCoroutineReturnObject &&_Other) noexcept;

		mark_artificial inline_always ~TCWrappedCoroutineReturnObject();

		mark_artificial inline_always operator TCWrapped<t_CType>();

		CHandle m_Handle;
	};

	template <typename t_CType>
	struct TCWrappedCoroutinePromiseBase
	{
		TCAsyncResult<t_CType> m_Result;

		mark_no_coroutine_debug TCWrappedCoroutineReturnObject<t_CType> get_return_object() noexcept;

		CSuspendAlways initial_suspend() noexcept;
		CSuspendAlways final_suspend() noexcept;

		void unhandled_exception() noexcept;

		template <typename tf_CType>
		TCWrappedSyncAwaiter<tf_CType> await_transform(TCAsyncResult<tf_CType> &&_Result) noexcept;

		template <typename tf_CType>
		TCWrappedSyncAwaiter<tf_CType> await_transform(TCWrapped<tf_CType> &&_Result) noexcept;
	};

	template <typename t_CType>
	struct TCWrappedCoroutinePromise : public TCWrappedCoroutinePromiseBase<t_CType>
	{
		void return_value(t_CType &&_Value);

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);
	};

	template <>
	struct TCWrappedCoroutinePromise<void> : public TCWrappedCoroutinePromiseBase<void>
	{
		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);

		void return_value(CVoidTag &&);
		void return_value(CVoidTag const &);
	};
}

namespace std
{
	template <typename t_CType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCWrapped<t_CType>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCWrappedCoroutinePromise<t_CType>;
	};

#ifdef _LIBCPP_HAS_EXTENDED_COROUTINE_TRAITS
	template <typename t_CType, typename ...tp_CParams>
	struct extended_coroutine_traits<NMib::NConcurrency::TCWrapped<t_CType>, tp_CParams...>
	{
		using promise_type = NMib::NConcurrency::NPrivate::TCWrappedCoroutinePromise<t_CType>;
	};
#endif
}

#include "Malterlib_Concurrency_WrappedCoroutine.hpp"
