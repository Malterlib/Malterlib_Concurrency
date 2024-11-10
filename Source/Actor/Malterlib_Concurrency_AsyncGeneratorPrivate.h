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

		inline_always void *operator new(std::size_t _Size, std::align_val_t _Alignment)
		{
			return NMib::NMemory::fg_AllocAligned(_Size, (mint)_Alignment);
		}

		inline_always void operator delete(void *_pMemory, std::size_t _Size, std::align_val_t _Alignment)
		{
			NMib::NMemory::fg_Free(_pMemory, _Size);
		}

		inline_always void *operator new(std::size_t _Size)
		{
			return NMib::NMemory::fg_Alloc(_Size);
		}

		inline_always void operator delete(void *_pMemory, std::size_t _Size)
		{
			NMib::NMemory::fg_Free(_pMemory, _Size);
		}

		TCAsyncGenerator<t_CReturnType> get_return_object();

		TCWeakActor<> m_AsyncGeneratorOwner;
		NStorage::TCSharedPointer<NAtomic::TCAtomic<bool>> m_pAborted;
	};

	template <typename t_CReturnType, ECoroutineFlag t_Flags>
	struct TCAsyncGeneratorCoroutineContextWithFlags : public TCAsyncGeneratorCoroutineContext<t_CReturnType>
	{
		TCAsyncGeneratorCoroutineContextWithFlags() noexcept
			: TCAsyncGeneratorCoroutineContext<t_CReturnType>(t_Flags)
		{
		}

		static constexpr ECoroutineFlag mc_InitialFlags = t_Flags;
	};
}

namespace std
{
#ifdef _LIBCPP_HAS_EXTENDED_COROUTINE_TRAITS
	template <typename t_CReturnType, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
			)
			|| !!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
	struct extended_coroutine_traits<NMib::NConcurrency::TCAsyncGenerator<t_CReturnType>, tp_CParams...>
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
#endif
	template <typename t_CReturnType, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & (NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters)
			)
			|| !!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
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
