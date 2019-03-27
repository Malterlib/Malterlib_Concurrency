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
		DMibFastCheck(fg_CurrentActor());
#endif

		auto &CoroutineContext = _Handle.promise();

		NPrivate::fg_CallActorInternal
			(
				mp_ActorCall
				, fg_CurrentActor()
				, [=, KeepAlive = CoroutineContext.f_KeepAliveImplicit()](TCAsyncResult<t_CReturnType> &&_Result) mutable
				{
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());

					mp_Result = fg_Move(_Result);
					if (!(mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Set) & ECoroutineAwaiterResult_Observed))
						return;

					auto &CoroutineContext = _Handle.promise();
#if DMibEnableSafeCheck > 0
					mp_pDebugException = CoroutineContext.f_Resume();
#else
					CoroutineContext.f_Resume();
#endif
					_Handle.resume();
				}
			)
		;

		if (mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Observed) & ECoroutineAwaiterResult_Set)
			return false;

		CoroutineContext.f_Suspend();
		return true;
	}

	template <typename tf_CType, typename tf_FExceptionTransform>
	auto fg_UnwrapCoroutineAsyncResult(TCAsyncResult<tf_CType> &&_AsyncResult, tf_FExceptionTransform &_fExceptionTransform)
	{
		if (!_AsyncResult)
		{
			try
			{
				_AsyncResult.f_Get();
			}
			catch (...)
			{
				NException::CDisableExceptionTraceScope DisableExceptionTrace;
				if constexpr (NTraits::TCIsSame<tf_FExceptionTransform, void *>::mc_Value)
					DMibErrorCoroutineWrapper("Exception calling async result", NException::fg_CurrentException());
				else
					DMibErrorCoroutineWrapper("Exception calling async result", _fExceptionTransform(NException::fg_CurrentException()));
			}
		}
		return fg_Move(*_AsyncResult);
	}

	template <typename t_CActorCall, typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	auto TCActorCallAwaiter<t_CActorCall, t_CReturnType, t_bUnwrap, t_FExceptionTransform>::await_resume()
#if DMibEnableSafeCheck == 0
		noexcept(!t_bUnwrap)
#endif
	{
#if DMibEnableSafeCheck > 0
		if (mp_pDebugException)
			std::rethrow_exception(mp_pDebugException);
#endif
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
		 	, TCActor<> const &_ResultActor
		 	, NMeta::TCIndices<tfp_Indices...>
		)
	{
		typedef NMeta::TCTypeList
			<
				typename NPrivate::TCGetResultType<typename NPrivate::TCGetActorCallFunctionPointer<tp_CCalls>::CType>::CType...
			> CTypeList
		;
		typedef NPrivate::TCCallMutipleActorStorage<false, tf_CResultFunctor, TCActor<>, CTypeList> CStorage;

#ifdef DMibContractConfigure_CheckEnabled
		auto Cleanup = g_OnScopeExit > [&]
			{
				fg_Swallow([this]{fg_Get<tfp_Indices>(mp_Calls).mp_Actor.f_Clear(); return 0;}()...);
			}
		;
#endif

		NStorage::TCSharedPointer<CStorage> pStorage = fg_Construct(_ResultActor, fg_Forward<tf_CResultFunctor>(_ResultFunctor));

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
		 	, NStr::CStr &o_Errors
		 	, NContainer::TCVector<NException::CExceptionPointer> &o_Exceptions
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
						o_Exceptions.f_Insert(SourceResult.f_GetException());
						bSuccess = false;
						NStr::fg_AddStrSep(o_Errors, SourceResult.f_GetExceptionStr(), "\n");
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
		DMibFastCheck(fg_CurrentActor());
#endif
		auto &CoroutineContext = _Handle.promise();

		fp_ActorCall
			(
				[=, KeepAlive = CoroutineContext.f_KeepAliveImplicit()](CWrappedType &&_Results) mutable
				{
					DMibFastCheck(CurrentActor->f_CurrentlyProcessing());
					mp_Results = fg_Move(_Results);

					if (!(mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Set) & ECoroutineAwaiterResult_Observed))
						return;

					auto &CoroutineContext = _Handle.promise();
#if DMibEnableSafeCheck > 0
					mp_pDebugException = CoroutineContext.f_Resume();
#else
					CoroutineContext.f_Resume();
#endif
					_Handle.resume();
				}
				, fg_CurrentActor()
				, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType()
			)
		;

		if (mp_ResultAvailable.f_FetchOr(ECoroutineAwaiterResult_Observed) & ECoroutineAwaiterResult_Set)
			return false;

		CoroutineContext.f_Suspend();
		return true;
	}

	template <bool t_bUnwrap, typename t_FExceptionTransform, typename... tp_CCalls>
	auto TCActorCallPackAwaiter<t_bUnwrap, t_FExceptionTransform, tp_CCalls...>::await_resume()
#if DMibEnableSafeCheck == 0
		noexcept(!t_bUnwrap)
#endif
	{
#if DMibEnableSafeCheck > 0
		if (mp_pDebugException)
			std::rethrow_exception(mp_pDebugException);
#endif
		if constexpr (t_bUnwrap)
		{
			CUnwrappedType Results;
			NContainer::TCVector<NException::CExceptionPointer> Exceptions;
			NStr::CStr ErrorStr;

			if
				(
					!fp_Unwrap
					(
						Results
						, ErrorStr
					 	, Exceptions
						, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CCalls)>::CType()
					)
				)
			{
				NException::CDisableExceptionTraceScope DisableExceptionTrace;
				if constexpr (NTraits::TCIsSame<t_FExceptionTransform, void *>::mc_Value)
					DMibErrorCoroutineWrapper("Exception calling async result", DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions), false).f_ExceptionPointer());
				else
					DMibErrorCoroutineWrapper("Exception calling async result", mp_fExceptionTransform(DMibErrorInstanceExceptionVector(ErrorStr, fg_Move(Exceptions), false).f_ExceptionPointer()));
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
		: CActorWithErrorBase{_Error}
		, mp_pActorCall{_pActorCall}
	{
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::f_Wrap() -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CActorCall, typename ...tp_CCalls>
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::operator co_await()
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
	auto TCActorCallPackWithError<t_CActorCall, tp_CCalls...>::CNoUnwrapAsyncResult::operator co_await()
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
		: CActorWithErrorBase{_Error}
		, mp_pActorCall{_pActorCall}
	{
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::f_Wrap() -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CActorCall, typename t_CReturnType>
	TCFutureWithError<t_CReturnType> TCActorCallWithError<t_CActorCall, t_CReturnType>::f_Future() &&
	{
		return fg_Move(*mp_pActorCall).f_Future() % this->m_Error;
	}

	template <typename t_CActorCall, typename t_CReturnType>
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::operator co_await()
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
	auto TCActorCallWithError<t_CActorCall, t_CReturnType>::CNoUnwrapAsyncResult::operator co_await()
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
