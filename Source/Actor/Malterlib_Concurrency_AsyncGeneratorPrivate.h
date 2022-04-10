  // Copyright © 2020 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CReturnType>
	struct TCAsyncGenerator;
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CReturnType>
	struct TCAsyncGeneratorData
	{
		TCAsyncGeneratorData(TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> &&_fGetNext);

		TCActorFunctor<TCFuture<NStorage::TCOptional<t_CReturnType>> ()> m_fGetNext;
		NStorage::TCOptional<t_CReturnType> m_LastValue;
	};

	template <typename t_CReturnType>
	struct TCAsyncGeneratorCoroutineContext : public TCFutureCoroutineContextShared<NStorage::TCOptional<t_CReturnType>>
	{
		TCAsyncGeneratorCoroutineContext(ECoroutineFlag _Flags = ECoroutineFlag_None) noexcept;

		struct CYieldSuspender
		{
			bool await_ready() const noexcept;

			template <typename tf_CPromise>
			inline bool await_suspend(TCCoroutineHandle<tf_CPromise> _Producer) noexcept;

			void await_resume() noexcept;
		};

		CSuspendAlways initial_suspend() noexcept
		{
			return {};
		}

		void return_value(CEmpty);

		template <typename tf_CReturnType>
		void return_value(tf_CReturnType &&_Value);

		template <typename tf_CReturnType>
		CYieldSuspender yield_value(tf_CReturnType &&_Value);
		CYieldSuspender yield_value(t_CReturnType &&_Value);

		TCAsyncGenerator<t_CReturnType> get_return_object();

		TCWeakActor<> m_AsyncGeneratorOwner;
		NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> m_pAborted;
		NStorage::TCSharedPointer<NFunction::TCFunctionMovable<void (TCAsyncResult<TCAsyncGenerator<t_CReturnType>> &&_AsyncResult)>> m_pKeepAliveParams;
	};

	template <typename t_CReturnType, ECoroutineFlag t_Flags>
	struct TCAsyncGeneratorCoroutineContextWithFlags : public TCAsyncGeneratorCoroutineContext<t_CReturnType>
	{
		TCAsyncGeneratorCoroutineContextWithFlags() noexcept
			: TCAsyncGeneratorCoroutineContext<t_CReturnType>(t_Flags)
		{
		}
	};

	template <typename t_CReturnValue, typename t_CException>
	struct TCRunProtectedFutureHelper<TCAsyncGenerator<t_CReturnValue>, t_CException>
	{
		using CReturn = TCFuture<t_CReturnValue>;

		template <typename tf_FToRun, typename ...tfp_CParams>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator () (tf_FToRun &&_ToRun, tfp_CParams && ...p_Params) const;

		template <typename tf_FToRun>
		mark_no_coroutine_debug TCFuture<t_CReturnValue> operator / (tf_FToRun &&_ToRun) const;
	};
}

#ifdef DMibCoroutineTS
namespace std::experimental
#else
namespace std
#endif
{
	template <typename t_CReturnType, typename ...tp_CParams>
	struct coroutine_traits<NMib::NConcurrency::TCAsyncGenerator<t_CReturnType>, tp_CParams...>
	{
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCAsyncGeneratorCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCAsyncGeneratorCoroutineContext<t_CReturnType>;
#endif
	};
}
