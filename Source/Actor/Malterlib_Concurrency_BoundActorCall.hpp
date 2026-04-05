// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_Bind(tfp_CCallParams &&... p_CallParams) const &
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCActor<t_CActor> const &
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, false
			, tfp_CCallParams...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
#if DMibEnableSafeCheck > 0
		using CMemberFunction = decltype(tf_pFunctionPointer);
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor> const &
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			{
				.mp_pMemberFunction = tf_pFunctionPointer
				, .mp_Actor{*this}
				, .mp_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_Bind(tfp_CCallParams &&... p_CallParams) &&
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCActor<t_CActor> &&
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, false
			, tfp_CCallParams...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
#if DMibEnableSafeCheck > 0
		using CMemberFunction = decltype(tf_pFunctionPointer);
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor> &&
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, false
				, tfp_CCallParams...
			>
			{
				.mp_pMemberFunction = tf_pFunctionPointer
				, .mp_Actor{fg_Move(*this)}
				, .mp_Params{p_CallParams...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_BindByValue(tfp_CCallParams &&... p_CallParams) const &
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCActor<t_CActor>
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, true
			, NTraits::TCDecay<tfp_CCallParams>...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
#if DMibEnableSafeCheck > 0
		using CMemberFunction = decltype(tf_pFunctionPointer);
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			{
				.mp_pMemberFunction = tf_pFunctionPointer
				, .mp_Actor{*this}
				, .mp_Params{fg_Forward<tfp_CCallParams>(p_CallParams)...}
			}
		;
	}

	template <typename t_CActor>
	template <auto tf_pFunctionPointer, CBindActorOptions tf_BindOptions, typename... tfp_CCallParams>
	auto TCActor<t_CActor>::f_BindByValue(tfp_CCallParams &&... p_CallParams) &&
		-> TCBoundActorCall
		<
			NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
			, TCActor<t_CActor>
			, decltype(tf_pFunctionPointer)
			, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
			, true
			, NTraits::TCDecay<tfp_CCallParams>...
		>
		requires cActorCallableWithFunctor<tf_pFunctionPointer, t_CActor, tfp_CCallParams...>
	{
#if DMibEnableSafeCheck > 0
		using CMemberFunction = decltype(tf_pFunctionPointer);
		using CMemberPointerTraits = typename NTraits::TCMemberFunctionPointerTraits<NTraits::TCRemoveReference<CMemberFunction>>;
#endif
		// If you get this you are probably calling an empty actor
		DMibFastCheck(!f_IsEmpty());

		// If you get this, use f_CallActor instead of calling directly
		DMibFastCheck
			(
				!NTraits::cIsMemberFunctionPointer<CMemberFunction>
				|| (NTraits::cIsSame<typename CMemberPointerTraits::CClass, CActor>)
				|| !(*this)->f_GetDistributedActorData()
				|| (*this)->f_GetDistributedActorData()->f_IsValidForCall()
			)
		;

		return TCBoundActorCall
			<
				NPrivate::TCFutureReturn<decltype(tf_pFunctionPointer)>
				, TCActor<t_CActor>
				, decltype(tf_pFunctionPointer)
				, CBindActorOptions(tf_BindOptions.m_CallType, NPrivate::gc_VirtualCallDetection<tf_pFunctionPointer, tf_BindOptions.m_VirtualCall>)
				, true
				, NTraits::TCDecay<tfp_CCallParams>...
			>
			{
				.mp_pMemberFunction = tf_pFunctionPointer
				, .mp_Actor{fg_Move(*this)}
				, .mp_Params{fg_Forward<tfp_CCallParams>(p_CallParams)...}
			}
		;
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_FOnResult>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (tf_FOnResult &&_fOnResult) &&
		requires (NTraits::cIsCallableWith<tf_FOnResult, void (TCAsyncResult<t_CReturn> &&_Result)>)
	{
		auto Actor = fg_CurrentActor();
		DMibFastCheck(Actor);

		return fg_Move(*this).fp_CallOnResult(fg_Move(Actor) / fg_Forward<tf_FOnResult>(_fOnResult), CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (CDiscardResult const &_Discard) &&
	{
		return fg_Move(*this).fp_CallDiscard(CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_DiscardResult() &&
	{
		return fg_Move(*this).fp_CallDiscard(CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (TCFutureVector<t_CReturn> &_FutureVector) &&
	{
		return fg_Move(*this).fp_CallOnResult(_FutureVector.fp_AddResult(), CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CKey, typename tf_CForwardKey>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator >
		(
			TCFutureMapBoundKey<tf_CKey, t_CReturn, tf_CForwardKey> &&_BoundKey
		) &&
	{
		return fg_Move(*this).fp_CallOnResult(_BoundKey.m_This.fp_AddResult(fg_Forward<tf_CForwardKey>(_BoundKey.m_Key)), CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (TCVectorOfFutures<t_CReturn> &_ResultVector) &&
	{
		_ResultVector.f_Insert(fg_Move(*this).fp_Call(CIndices()));
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (NContainer::TCMapResult<TCFuture<t_CReturn> &> &&_MapResult) &&
	{
		*_MapResult = fg_Move(*this).fp_Call(CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	TCFuture<t_CReturn> TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_Call() &&
	{
		return fg_Move(*this).fp_Call(CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator TCFuture<t_CReturn> () &&
	{
		return fg_Move(*this).fp_Call(CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_CallSync() &&
	{
		return fg_Move(*this).fp_Call(CIndices()).f_CallSync();
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_CallSync(fp64 _Timeout) &&
	{
		return fg_Move(*this).fp_Call(CIndices()).f_CallSync(_Timeout);
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_CallSync
		(
			NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop
			, fp64 _Timeout
		) &&
	{
		return fg_Move(*this).fp_Call(CIndices()).f_CallSync(_pRunLoop, _Timeout);
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <umint ...tfp_Indices>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::fp_CallDiscard(NMeta::TCIndices<tfp_Indices...> _Indices) &&
	{
		if constexpr (t_bByValue)
		{
			NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
				(
					fg_Forward<t_CActor>(mp_Actor)
					, mp_pMemberFunction
					, CPromiseConstructDiscardResult()
					, fg_Move(fg_Get<tfp_Indices>(mp_Params))...
				)
			;
		}
		else
		{
			NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
				(
					fg_Forward<t_CActor>(mp_Actor)
					, mp_pMemberFunction
					, CPromiseConstructDiscardResult()
					, fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(mp_Params))...
				)
			;
		}
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <umint ...tfp_Indices>
	TCFuture<t_CReturn> TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::fp_Call(NMeta::TCIndices<tfp_Indices...> _Indices) &&
	{
		if constexpr (t_bByValue)
		{
			return NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
				(
					fg_Forward<t_CActor>(mp_Actor)
					, mp_pMemberFunction
					, CPromiseConstructNoConsume()
					, fg_Move(fg_Get<tfp_Indices>(mp_Params))...
				)
			;
		}
		else
		{
			return NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
				(
					fg_Forward<t_CActor>(mp_Actor)
					, mp_pMemberFunction
					, CPromiseConstructNoConsume()
					, fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(mp_Params))...
				)
			;
		}
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_FOnResult, umint ...tfp_Indices>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::fp_CallOnResult
		(
			tf_FOnResult &&_fOnResult
			, NMeta::TCIndices<tfp_Indices...> _Indices
		) &&
	{
#ifdef DMibCheckOnResultSizes
		if constexpr (sizeof(_fOnResult) > gc_FutureOnResultStorage)
		{
			if constexpr (t_bByValue)
			{
				NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
					(
						fg_Forward<t_CActor>(mp_Actor)
						, mp_pMemberFunction
						, TCFutureOnResultOverSized<t_CReturn>{fg_Move(_fOnResult)}
						, fg_Move(fg_Get<tfp_Indices>(mp_Params))...
					)
				;
			}
			else
			{
				NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
					(
						fg_Forward<t_CActor>(mp_Actor)
						, mp_pMemberFunction
						, TCFutureOnResultOverSized<t_CReturn>{fg_Move(_fOnResult)}
						, fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(mp_Params))...
					)
				;
			}
		}
		else
#endif
		{
			if constexpr (t_bByValue)
			{
				NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
					(
						fg_Forward<t_CActor>(mp_Actor)
						, mp_pMemberFunction
						, fg_Move(_fOnResult)
						, fg_Move(fg_Get<tfp_Indices>(mp_Params))...
					)
				;
			}
			else
			{
				NPrivate::fg_CallActorFuture<t_BindOptions.m_CallType == EActorCallType::mc_Direct, t_BindOptions.m_VirtualCall>
					(
						fg_Forward<t_CActor>(mp_Actor)
						, mp_pMemberFunction
						, fg_Move(_fOnResult)
						, fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(mp_Params))...
					)
				;
			}
		}
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator co_await() &&
		-> TCBoundActorCallAwaiter<t_CReturn, TCBoundActorCall, true, CVoidTag>
	{
		return {fg_Move(*this), {}};
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::CNoUnwrapAsyncResult::operator co_await() &&
		-> TCBoundActorCallAwaiter<t_CReturn, TCBoundActorCall, false, CVoidTag>
	{
		return {fg_Move(*m_pWrapped), {}};
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CResultActor, typename tf_CResultFunctor>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator >
		(
			TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall
		) &&
		requires (NTraits::cIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<t_CReturn> &&)>) // Incorrect type in result call
	{
		DMibFastCheck(_ResultCall.mp_Actor);

		return fg_Move(*this).fp_CallOnResult(fg_Move(_ResultCall), CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_FOnResult>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (TCDirectResultFunctor<tf_FOnResult> &&_fOnResult) &&
	{
		return fg_Move(*this).fp_CallOnResult(fg_Move(_fOnResult.m_fOnResult), CIndices());
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CReturnValue>
	inline_always void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator > (TCPromise<tf_CReturnValue> &&_Promise) &&
	{
		fg_Move(*this).f_OnResultSet
			(
				[Promise = fg_Move(_Promise)](TCAsyncResult<t_CReturn> &&_Result)
				{
					Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CResult>
	void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator >
		(
			TCPromiseWithExceptionTransfomer<tf_CResult> &&_PromiseWithError
		) &&
	{
		fg_Move(*this).f_OnResultSet
			(
				[Promise = fg_Move(_PromiseWithError.m_Promise), fTransformer = fg_Move(_PromiseWithError).f_GetTransformer()](TCAsyncResult<t_CReturn> &&_Result) mutable
				{
					if (!_Result)
						Promise.f_SetException(fTransformer(_Result.f_GetException()));
					else
						Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator % (NStr::CStr const &_ErrorString) &&
		-> TCFutureWithExceptionTransformer<t_CReturn, TCBoundActorCall>
	{
		return TCFutureWithExceptionTransformer<t_CReturn, TCBoundActorCall>(fg_Move(*this), fg_ExceptionTransformer(nullptr, _ErrorString));
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CType>
	inline_always auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator + (TCFuture<tf_CType> &&_Other) &&
	{
		return TCFuturePack<TCBoundActorCall, TCFuture<tf_CType>>(NStorage::fg_Tuple(fg_Move(*this), fg_Move(_Other)));
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	inline_always auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator +
		(
			TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other
		) &&
	{
		return TCFuturePack<TCBoundActorCall, TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
			(
				NStorage::fg_Tuple(fg_Move(*this), fg_Move(_Other))
			)
		;
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_CReturn, typename tf_CFutureLike>
	inline_always auto TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::operator +
		(
			TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other
		) &&
	{
		return fg_Move(*this) + fg_Move(_Other).f_Dispatch();
	}

	template <typename t_CReturn, typename t_CActor, typename t_FFunctionPointer, CBindActorOptions t_BindOptions, bool t_bByValue, typename ...tp_CParams>
	template <typename tf_FOnResult>
	inline_always void TCBoundActorCall<TCFuture<t_CReturn>, t_CActor, t_FFunctionPointer, t_BindOptions, t_bByValue, tp_CParams...>::f_OnResultSet(tf_FOnResult &&_fOnResult) &&
	{
		fg_Move(*this).fp_CallOnResult(fg_Forward<tf_FOnResult>(_fOnResult), CIndices());
	}

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	TCBoundActorCallAwaiter<t_CReturn, t_CBoundFunctor, t_bUnwrap, t_FExceptionTransform>::TCBoundActorCallAwaiter
		(
			t_CBoundFunctor &&_BoundFunctor
			, t_FExceptionTransform &&_fExceptionTransform
		)
		: mp_BoundFunctor(fg_Move(_BoundFunctor))
		, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
	{
	}

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	TCBoundActorCallAwaiter<t_CReturn, t_CBoundFunctor, t_bUnwrap, t_FExceptionTransform>::~TCBoundActorCallAwaiter()
	{
	}

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	bool TCBoundActorCallAwaiter<t_CReturn, t_CBoundFunctor, t_bUnwrap, t_FExceptionTransform>::await_ready() noexcept
	{
		return false;
	}

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	template <typename tf_CCoroutineContext>
	bool TCBoundActorCallAwaiter<t_CReturn, t_CBoundFunctor, t_bUnwrap, t_FExceptionTransform>::await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		noexcept(NTraits::cIsSame<t_FExceptionTransform, CVoidTag>)
	{
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		DMibFastCheck(fg_CurrentActor(ThreadLocal));

		auto &CoroutineContext = _Handle.promise();

		fg_Move(mp_BoundFunctor) > g_DirectResult
			/
			[
				this
				, KeepAlive = CoroutineContext.f_KeepAlive(fg_ThisActor(ThreadLocal.m_pCurrentlyProcessingActorHolder))
			]
			(TCAsyncResult<t_CReturn> &&_Result) mutable
			{
				auto Actor = KeepAlive.f_MoveActor();
				if (!Actor)
					return;

				auto pActor = Actor.f_Get();

				pActor->f_QueueProcess
					(
						[
							this
							, KeepAlive = KeepAlive.f_MoveImplicit()
#if DMibEnableSafeCheck > 0
							, pActor
#endif
							, Result = fg_Move(_Result)
						]
						(CConcurrencyThreadLocal &_ThreadLocal) mutable
						{
							DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == pActor);

							if (!KeepAlive.f_HasValidCoroutine())
								return; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

							auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();

							mp_AsyncResult = fg_Move(Result);

							auto &CoroutineContext = Handle.promise();
							if constexpr (t_bUnwrap)
							{
								if (!mp_AsyncResult)
								{
									NStorage::TCUniquePointer<COnResumeException> pResult = fg_Construct(fg_TransformException(fg_Move(mp_AsyncResult).f_GetException(), mp_fExceptionTransform));
									CoroutineContext.f_HandleAwaitedResult
										(
											_ThreadLocal
											, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor
											, fg_Move(pResult)
										)
									;
									return;
								}
							}

							{
								bool bAborted = false;
								auto RestoreStates = CoroutineContext.f_Resume(_ThreadLocal.m_SystemThreadLocal, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor, bAborted);
								if (!bAborted)
									Handle.resume();
							}
						}
					)
				;
			}
		;

		CoroutineContext.f_Suspend(ThreadLocal.m_SystemThreadLocal, true);
		return true;
	}

	template <typename t_CReturn, typename t_CBoundFunctor, bool t_bUnwrap, typename t_FExceptionTransform>
	auto TCBoundActorCallAwaiter<t_CReturn, t_CBoundFunctor, t_bUnwrap, t_FExceptionTransform>::await_resume() noexcept
	{
		if constexpr (t_bUnwrap)
		{
			DMibFastCheck(mp_AsyncResult);
			return fg_Move(*mp_AsyncResult);
		}
		else
		{
			if constexpr (NTraits::cIsSame<t_FExceptionTransform, CVoidTag>)
				return fg_Move(mp_AsyncResult);
			else
			{
				if (mp_AsyncResult)
					return fg_Move(mp_AsyncResult);
				else
				{
					TCAsyncResult<t_CReturn> Result;
					Result.f_SetException(mp_fExceptionTransform(fg_Move(mp_AsyncResult.f_GetException())));
					return Result;
				}
			}
		}
	}
}

