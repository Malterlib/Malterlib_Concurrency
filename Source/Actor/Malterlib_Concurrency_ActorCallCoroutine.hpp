// Copyright © 2018 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::TCActorCallAwaiter(t_CActorCall &&_ActorCall, t_FExceptionTransform &&_fExceptionTransform)
		: mp_ActorCall(fg_Move(_ActorCall))
		, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
	{
	}

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::~TCActorCallAwaiter()
	{
		mp_ActorCall.mp_Actor.f_Clear();
	}

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	bool TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::await_ready() const noexcept
	{
		return false;
	}

	enum ECoroutineAwaiterResult
	{
		ECoroutineAwaiterResult_None = 0
		, ECoroutineAwaiterResult_Observed = DMibBit(0)
		, ECoroutineAwaiterResult_Set = DMibBit(1)
	};

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	template <typename tf_CCoroutineContext>
	bool TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
	{
#if DMibEnableSafeCheck > 0
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		DMibFastCheck(fg_CurrentActorProcessingOrOverridden());
#endif

		auto &CoroutineContext = _Handle.promise();

		NPrivate::fg_CallActorInternal
			(
				mp_ActorCall
				, fg_CurrentActor()
				, [=, KeepAlive = CoroutineContext.f_KeepAliveImplicit()](TCAsyncResult<t_CReturnType> &&_Result) mutable
				{
#if DMibEnableSafeCheck > 0
					if (!KeepAlive.f_HasValidCoroutine())
						return; // Can happen when f_Suspend throws

					auto CurrentActor = fg_CurrentActor();
					DMibFastCheck(CurrentActor);
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
#endif

					mp_Result = fg_Move(_Result);
					if (!(mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Set) & ECoroutineAwaiterResult_Observed))
						return;

					auto &CoroutineContext = _Handle.promise();
					bool bAborted = false;
					auto RestoreStates = CoroutineContext.f_Resume(bAborted, !mp_Result);
					if (!bAborted)
						_Handle.resume();
				}
			)
		;

		if (mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Observed) & ECoroutineAwaiterResult_Set)
			return false;

		CoroutineContext.f_Suspend(true);
		return true;
	}

	void fg_ThrowExceptionCallingAsyncResult(NException::CExceptionPointer &&_Exception);

	template <typename tf_CType, typename tf_FExceptionTransform>
	auto fg_UnwrapCoroutineAsyncResult(TCAsyncResult<tf_CType> &&_AsyncResult, tf_FExceptionTransform &_fExceptionTransform)
	{
		if (!_AsyncResult)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			if constexpr (NTraits::TCIsSame<tf_FExceptionTransform, void *>::mc_Value)
				fg_ThrowExceptionCallingAsyncResult(fg_Move(_AsyncResult).f_GetException());
			else
				fg_ThrowExceptionCallingAsyncResult(_fExceptionTransform(fg_Move(_AsyncResult).f_GetException()));
		}
		return fg_Move(*_AsyncResult);
	}

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	auto TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::await_resume() noexcept(!t_bUnwrap)
	{
		if constexpr (t_bUnwrap)
			return fg_UnwrapCoroutineAsyncResult(fg_Move(mp_Result), mp_fExceptionTransform);
		else
		{
			if constexpr (NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
				return fg_Move(mp_Result);
			else
			{
				if (mp_Result)
					return fg_Move(mp_Result);
				else
				{
					TCAsyncResult<t_CReturnType> Result;
					Result.f_SetException(mp_fExceptionTransform(fg_Move(mp_Result.f_GetException())));
					return Result;
				}
			}
		}
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::TCActorCallPackAwaiter(NStorage::TCTuple<tp_CCalls...> &&_Calls, t_FExceptionTransform &&_fExceptionTransform)
		: mp_Calls(fg_Move(_Calls))
		, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
	{
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::~TCActorCallPackAwaiter()
	{
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	template
	<
		typename tf_CResultFunctor
		, mint... tfp_Indices
	>
	void TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::fp_ActorCall
		(
		 	tf_CResultFunctor &&_ResultFunctor
		 	, TCActor<> &&_ResultActor
		 	, NMeta::TCIndices<tfp_Indices...>
		)
	{
		typedef NMeta::TCTypeList
			<
				typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType...
			> CTypeList
		;
		typedef NPrivate::TCCallMutipleActorStorage<false, tf_CResultFunctor, TCActor<>, CTypeList> CStorage;

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(fg_Move(_ResultActor), fg_Forward<tf_CResultFunctor>(_ResultFunctor));

		auto &DirectActor = NPrivate::fg_DirectResultActor();
		TCInitializerList<bool> Dummy =
			{
				NPrivate::fg_CallActorInternal
				(
					fg_Get<tfp_Indices>(mp_Calls)
					, fg_TempCopy(DirectActor)
					, [pStorage](TCAsyncResult<typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType> &&_Result)
					{
						auto &Internal = *pStorage;
						fg_Get<tfp_Indices>(Internal.m_Results) = fg_Move(_Result);
						Internal.f_Finished();
					}
				)...
			}
		;
		(void)Dummy;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	template <mint... tfp_Indices>
	bool TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::fp_Unwrap
		(
		 	CUnwrappedType &o_Results
			, NException::CExceptionExceptionVectorData::CErrorCollector &o_ErrorCollector
		 	, NMeta::TCIndices<tfp_Indices...>
		)
	{
		bool bSuccess = true;
		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					auto &SourceResult = fg_Get<tfp_Indices>(mp_Results);
					if (!SourceResult)
					{
						bSuccess = false;
						o_ErrorCollector.f_AddError(SourceResult.f_GetException());
					}
					else
						fg_Get<tfp_Indices>(o_Results) = fg_Move(*SourceResult);
					return false;
				}
				()...
			}
		;
		(void)Dummy;
		return bSuccess;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	template <mint... tfp_Indices>
	bool TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::fp_HasException(NMeta::TCIndices<tfp_Indices...>)
	{
		bool bSuccess = true;
		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					if (!fg_Get<tfp_Indices>(mp_Results))
						bSuccess = false;
					return false;
				}
				()...
			}
		;
		(void)Dummy;
		return !bSuccess;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	template <mint... tfp_Indices>
	void TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::fp_TransformExceptions(CWrappedType &o_Results, NMeta::TCIndices<tfp_Indices...>)
	{
		TCInitializerList<bool> Dummy =
			{
				[&]
				{
					auto &Result = fg_Get<tfp_Indices>(o_Results);
					if (!Result)
						Result.f_SetException(mp_fExceptionTransform(Result.f_GetException()));
					return false;
				}
				()...
			}
		;
		(void)Dummy;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	bool TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::await_ready() const noexcept
	{
		return false;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	template <typename tf_CCoroutineContext>
	bool TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
	{
#if DMibEnableSafeCheck > 0
		auto CurrentActor = fg_CurrentActor();
		DMibFastCheck(CurrentActor);
		DMibFastCheck(fg_CurrentActorProcessingOrOverridden());
#endif
		auto &CoroutineContext = _Handle.promise();

		fp_ActorCall
			(
				[=, KeepAlive = CoroutineContext.f_KeepAliveImplicit()](CWrappedType &&_Results) mutable
				{
#if DMibEnableSafeCheck > 0
					if (!KeepAlive.f_HasValidCoroutine())
						return; // Can happen when f_Suspend throws

					auto CurrentActor = fg_CurrentActor();
					DMibFastCheck(CurrentActor);
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
#endif
					mp_Results = fg_Move(_Results);

					if (!(mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Set) & ECoroutineAwaiterResult_Observed))
						return;

					auto &CoroutineContext = _Handle.promise();
					bool bAborted = false;
					auto RestoreStates = CoroutineContext.f_Resume(bAborted, fp_HasException(typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType()));
					if (!bAborted)
						_Handle.resume();
				}
				, fg_CurrentActor()
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType()
			)
		;

		if (mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Observed) & ECoroutineAwaiterResult_Set)
			return false;

		CoroutineContext.f_Suspend(true);
		return true;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	auto TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::await_resume() noexcept(!t_bUnwrap)
	{
		if constexpr (t_bUnwrap)
		{
			CUnwrappedType Results;
			NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;

			if
				(
					!fp_Unwrap
					(
						Results
						, ErrorCollector
						, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType()
					)
				)
			{
				NException::CDisableExceptionTraceScope DisableExceptionTrace;
				if constexpr (NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
					fg_ThrowExceptionCallingAsyncResult(fg_Move(ErrorCollector).f_GetException());
				else
					fg_ThrowExceptionCallingAsyncResult(mp_fExceptionTransform(fg_Move(ErrorCollector).f_GetException()));
			}

			return Results;
		}
		else
		{
			if constexpr (!NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
				fp_TransformExceptions(mp_Results, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType());
			return fg_Move(mp_Results);
		}
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::TCActorCallPackWithError(t_CActorCall *_pActorCall, NStr::CStr const &_Error)
		: CActorWithErrorBase(_Error)
		, mp_pActorCall{_pActorCall}
	{
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::operator co_await() &&
	{
		return TCActorCallPackAwaiter
			<
				true
				, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>
				, tp_CCalls...
			>
			{fg_Move(mp_pActorCall->m_Calls), this->fp_GetTransformer()}
		;
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCActorCallPackAwaiter
			<
				false
				, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>
				, tp_CCalls...
			>
			{fg_Move(m_pWrapped->mp_pActorCall->m_Calls), m_pWrapped->fp_GetTransformer()}
		;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	TCActorCallWithError<t_CActorCall, t_CReturnType>::TCActorCallWithError(t_CActorCall *_pActorCall, NStr::CStr const &_Error)
		: CActorWithErrorBase(_Error)
		, mp_pActorCall{_pActorCall}
	{
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CActorCall, typename t_CReturnType>
	TCFutureWithError<t_CReturnType> TCActorCallWithError<t_CActorCall, t_CReturnType>::f_Future() &&
	{
		return fg_Move(*mp_pActorCall).f_Future() % this->m_Error;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	template <typename tf_CRight>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::operator > (tf_CRight &&_Right) &&
	{
		return fg_Move(*this).f_Dispatch() > fg_Forward<tf_CRight>(_Right);
	}

	template <typename t_CActorCall, typename t_CReturnType>
	TCFuture<t_CReturnType> TCActorCallWithError<t_CActorCall, t_CReturnType>::f_Dispatch() &&
	{
		TCPromise<t_CReturnType> Promise;
		fg_Move(*mp_pActorCall) > NPrivate::fg_DirectResultActor() / [Promise, fTransformer = fp_GetTransformer()](TCAsyncResult<t_CReturnType> &&_Result)
			{
				if (!_Result)
					return Promise.f_SetException(fTransformer(fg_Move(_Result).f_GetException()));

				if constexpr (NTraits::TCIsVoid<t_CReturnType>::mc_Value)
					Promise.f_SetResult();
				else
					Promise.f_SetResult(fg_Move(*_Result));
			}
		;

		return Promise.f_MoveFuture();
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::operator co_await() &&
	{
		return TCActorCallAwaiter
			<
				t_CActorCall
				, t_CReturnType
				, true
				, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>
			>
			{fg_Move(*mp_pActorCall), this->fp_GetTransformer()}
		;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCActorCallAwaiter
			<
				t_CActorCall
				, t_CReturnType
				, false
				, NFunction::TCFunction<NException::CExceptionPointer (NException::CExceptionPointer &&_pException)>
			>
			{fg_Move(*m_pWrapped->mp_pActorCall), m_pWrapped->fp_GetTransformer()}
		;
	}
}
