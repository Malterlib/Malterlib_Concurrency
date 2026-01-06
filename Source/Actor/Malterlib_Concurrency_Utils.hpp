// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once
#include "Malterlib_Concurrency_WeakActor.h"

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		constexpr static mint const gc_ActorResultFinishedMask = DMibBitRangeTyped(0, sizeof(mint)*8-2, mint);
		constexpr static mint const gc_ActorResultResultsGottenMask = DMibBitRangeTyped(sizeof(mint)*8-1, sizeof(mint)*8-1, mint);
	}

	template <typename tf_CType>
	bool fg_AnyFailed(NContainer::TCVector<TCAsyncResult<tf_CType>> const &_Results)
	{
		for (auto &Result : _Results)
		{
			if (!Result)
				return true;
		}
		return false;
	}

	template <typename tf_CType, typename tf_FOnResult>
	void fg_CombineResults
		(
			NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			throw fg_Move(ErrorCollector).f_GetException();
		}
	}

	template <typename tf_FOnResult>
	void fg_CombineResults
		(
			NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			throw fg_Move(ErrorCollector).f_GetException();
		}
	}

	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult();
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(fg_Move(*Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCVector<TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult();
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}


	template <typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCVector<TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult();
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}



	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
	void fg_CombineResults
		(
			NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			throw fg_Move(ErrorCollector).f_GetException();
		}
	}

	template <typename tf_CKey, typename tf_FOnResult>
	void fg_CombineResults
		(
			NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results->fs_GetKey(Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			NException::CDisableExceptionTraceScope DisableExceptionTrace;
			throw fg_Move(ErrorCollector).f_GetException();
		}
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result), fg_Move(*Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		if (!_Results)
		{
			_Promise.f_SetException(_Results.f_GetException());
			return false;
		}
		else
		{
			for (auto &Result : *_Results)
			{
				if (Result)
					_fOnResult(_Results->fs_GetKey(Result));
				else
				{
					bIsError = true;
					ErrorCollector.f_AddError(Result.f_GetException());
				}
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CReturn, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<tf_CReturn> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_CType, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CType>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result), fg_Move(*Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	template <typename tf_CKey, typename tf_FOnResult>
	bool fg_CombineResults
		(
			TCPromise<void> const &_Promise
			, NContainer::TCMap<tf_CKey, TCAsyncResult<void>> &&_Results
			, tf_FOnResult &&_fOnResult
		)
	{
		NException::CExceptionExceptionVectorData::CErrorCollector ErrorCollector;
		bool bIsError = false;
		for (auto &Result : _Results)
		{
			if (Result)
				_fOnResult(_Results.fs_GetKey(Result));
			else
			{
				bIsError = true;
				ErrorCollector.f_AddError(Result.f_GetException());
			}
		}
		if (bIsError)
		{
			_Promise.f_SetException(fg_Move(ErrorCollector).f_GetException());
			return false;
		}
		return true;
	}

	namespace NPrivate
	{
		template <bool tf_bDirect, typename tf_CActor, typename tf_FToDispatch, typename ...tfp_CFunctionParams, typename ...tfp_CParams>
		inline_always auto fg_DispatchGenericImpl(NMeta::TCTypeList<tfp_CFunctionParams...> _TypeList, tf_CActor &&_Actor, tf_FToDispatch &&_fDispatch, tfp_CParams && ...p_Params)
		{
			using CActorDereferenced = NTraits::TCRemoveReference<tf_CActor>;
			using CCallActor = NTraits::TCCopyQualifiersAndReference
				<
					tf_CActor &&
					, TCConditional<CActorDereferenced::mc_bIsAlwaysAlive || CActorDereferenced::mc_bIsWeak, NTraits::TCRemoveReference<tf_CActor>, TCActor<>>
				>
			;

			static_assert
				(
					NTraits::cIsCallableWith
					<
						NTraits::TCRemoveReference<tf_FToDispatch>
						, void (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>...)
					>
				)
			;
			using CReturnType = NTraits::TCCallableReturnTypeFor
				<
					NTraits::TCRemoveReference<tf_FToDispatch>
					, void (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CParams>...)
				>
			;

			if constexpr (NPrivate::TCIsFuture<CReturnType>::mc_Value || NPrivate::TCIsAsyncGenerator<CReturnType>::mc_Value)
			{
				if constexpr (tf_bDirect)
				{
					return reinterpret_cast<CCallActor>(_Actor).template f_BindByValue
						<
							&CActor::f_DispatchWithReturn<CReturnType, NTraits::TCDecay<tfp_CFunctionParams>...>
							, CBindActorOptions{EActorCallType::mc_Direct, EVirtualCall::mc_NotVirtual}
						>
						(
							NFunction::TCFunctionMovable<CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CFunctionParams>...)>
							(fg_Forward<tf_FToDispatch>(_fDispatch))
							, fg_Forward<tfp_CParams>(p_Params)...
						)
					;
				}
				else
				{
					return reinterpret_cast<CCallActor>(_Actor).template f_BindByValue<&CActor::f_DispatchWithReturn<CReturnType, tfp_CFunctionParams...>, EVirtualCall::mc_NotVirtual>
						(
							NFunction::TCFunctionMovable<CReturnType (NTraits::TCRemoveQualifiersAndAddRValueReference<tfp_CFunctionParams>...)>
							(fg_Forward<tf_FToDispatch>(_fDispatch))
							, fg_Forward<tfp_CParams>(p_Params)...
						)
					;
				}
			}
			else
			{
				static_assert(!NPrivate::TCIsPromise<CReturnType>::mc_Value);

				using CInnerReturn = typename TCGetReturnType<CReturnType>::CType;
				static_assert(!tf_bDirect); // Not implemented

				return reinterpret_cast<CCallActor>(_Actor).template f_BindByValue
					<
						&CActor::f_DispatchWithReturn<TCFuture<CInnerReturn>, tfp_CFunctionParams...>
						, EVirtualCall::mc_NotVirtual
					>
					(
						[fDispatch = fg_Forward<tf_FToDispatch>(_fDispatch)]<typename ...tfp_CParams2>(tfp_CParams2 ...p_InnerParams) mutable -> TCFuture<CInnerReturn>
						{
#ifdef DCompiler_clang_cl
							auto fWorkaroundExceptions = [&]() inline_never mutable
								{
									return fDispatch(fg_Move(p_InnerParams)...);
								}
							;

							try
							{
								if constexpr (NTraits::cIsVoid<CInnerReturn>)
								{
									fWorkaroundExceptions();
									co_return {};
								}
								else
									co_return fWorkaroundExceptions();
							}
							catch (...)
							{
								co_return NException::fg_CurrentException();
							}
#else
							try
							{
								if constexpr (NTraits::cIsVoid<CInnerReturn>)
								{
									fDispatch(fg_Move(p_InnerParams)...);
									co_return {};
								}
								else
									co_return fDispatch(fg_Move(p_InnerParams)...);
							}
							catch (...)
							{
								co_return NException::fg_CurrentException();
							}
#endif
						}
						, fg_Forward<tfp_CParams>(p_Params)...
					)
				;
			}
		}
	}

	template <typename tf_CActor, typename tf_FToDispatch, typename ...tfp_CParams>
	inline_always auto fg_Dispatch(TCActor<tf_CActor> const &_Actor, tf_FToDispatch &&_fDispatch, tfp_CParams && ...p_Params)
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_FToDispatch>;

		return NPrivate::fg_DispatchGenericImpl<false>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, _Actor
				, fg_Forward<tf_FToDispatch>(_fDispatch)
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CActor, typename tf_FToDispatch, typename ...tfp_CParams>
	inline_always auto fg_Dispatch(TCActor<tf_CActor> &&_Actor, tf_FToDispatch &&_fDispatch, tfp_CParams && ...p_Params)
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_FToDispatch>;

		return NPrivate::fg_DispatchGenericImpl<false>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, fg_Move(_Actor)
				, fg_Forward<tf_FToDispatch>(_fDispatch)
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_FToDispatch, typename ...tfp_CParams>
	inline_always auto fg_DirectDispatch(tf_FToDispatch &&_fDispatch, tfp_CParams && ...p_Params)
	{
		using CFunctionType = NTraits::TCRemoveReference<tf_FToDispatch>;

		return NPrivate::fg_DispatchGenericImpl<true>
			(
				typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
				, fg_CurrentActor()
				, fg_Forward<tf_FToDispatch>(_fDispatch)
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_FToDispatch>
	inline_always auto fg_Dispatch(tf_FToDispatch &&_fDispatch)
	{
		return NPrivate::fg_DispatchGenericImpl<false>
			(
				NMeta::TCTypeList<>()
				, fg_CurrentActor()
				, fg_Forward<tf_FToDispatch>(_fDispatch)
			)
		;
	}

	template <typename tf_FToDispatch>
	inline_always auto fg_ConcurrentDispatch(tf_FToDispatch &&_fDispatch)
	{
		return NPrivate::fg_DispatchGenericImpl<false>
			(
				NMeta::TCTypeList<>()
				, fg_ConcurrentActor()
				, fg_Forward<tf_FToDispatch>(_fDispatch)
			)
		;
	}

	namespace NPrivate
	{
		template <typename tf_FFunction>
		inline_always auto CThisActor::operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<true>
				(
					NMeta::TCTypeList<>()
					, fg_CurrentActor()
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}

		template <typename tf_FFunction, typename... tfp_CCallParams>
		inline_always auto CThisActor::f_Invoke(tf_FFunction &&_fFunction, tfp_CCallParams && ...p_CallParams) const
		{
			using CFunctionType = NTraits::TCRemoveReference<tf_FFunction>;

			return NPrivate::fg_DispatchGenericImpl<true>
				(
					typename NTraits::TCMemberFunctionPointerTraits<decltype(&CFunctionType::operator ())>::CParams()
					, fg_CurrentActor()
					, fg_Forward<tf_FFunction>(_fFunction)
					, fg_Forward<tfp_CCallParams>(p_CallParams)...
				)
			;
		}
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_CallSync() &&
	{
		TCAsyncResult<t_CReturnValue> Result;
		NThread::CEvent WaitEvent;
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[&](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					Result = fg_Move(_Result);
					WaitEvent.f_SetSignaled();
				}
			)
		;

		WaitEvent.f_Wait();

		return Result.f_Move();
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_CallSync(fp64 _Timeout) &&
	{
		NStorage::TCSharedPointer<NPrivate::TCCallSyncState<t_CReturnValue>> pResult = fg_Construct();
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[pResult](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					pResult->f_SetResult(fg_Move(_Result));
				}
			)
		;

		if (pResult->m_RunLoop.f_WaitTimeout(_Timeout))
		{
			NStr::CStr CallstackStr;
			{
				DMibLock(pResult->m_Lock);
				CallstackStr = pResult->m_Result.f_GetExceptionCallstackStr(0);
			}

			if (CallstackStr)
				DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to finish. Call stack:\n{}", CallstackStr));
			else
				DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to finish"));
		}

		DMibLock(pResult->m_Lock);
		return pResult->m_Result.f_Move();
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_CallSync(NStorage::TCSharedPointer<CRunLoop> const &_pRunLoop, fp64 _Timeout) &&
	{
		NStorage::TCSharedPointer<NPrivate::TCCallSyncState<t_CReturnValue, NPrivate::CRunLoopState>> pResult = fg_Construct(_pRunLoop);
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[pResult](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					pResult->f_SetResult(fg_Move(_Result));
				}
			)
		;

		if (_Timeout >= 0.0)
		{
			if (pResult->m_RunLoop.f_WaitTimeout(_Timeout))
			{
				NStr::CStr CallstackStr;
				{
					DMibLock(pResult->m_Lock);
					CallstackStr = pResult->m_Result.f_GetExceptionCallstackStr(0);
				}

				if (CallstackStr)
					DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to finish. Call stack:\n{}", CallstackStr));
				else
					DMibError(NStr::fg_Format("Timed out waiting for synchronous actor call to finish"));
			}
		}
		else
			pResult->m_RunLoop.f_Wait();

		DMibLock(pResult->m_Lock);
		return pResult->m_Result.f_Move();
	}

	struct CDispatchHelperWithActor
	{
		CDispatchHelperWithActor(TCActor<> const &_Actor)
			: m_Actor(_Actor)
		{
		}

		template <typename tf_FFunction>
		inline_always auto operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<false>
				(
					NMeta::TCTypeList<>()
					, m_Actor
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}

		TCActor<> m_Actor;
	};

	struct CDispatchHelperWithWeakActor
	{
		CDispatchHelperWithWeakActor(TCWeakActor<> const &_Actor)
			: m_Actor(_Actor)
		{
		}

		template <typename tf_FFunction>
		inline_always auto operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<false>
				(
					NMeta::TCTypeList<>()
					, m_Actor
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}

		TCWeakActor<> m_Actor;
	};

	struct CDispatchHelper
	{
		template <typename tf_FFunction>
		inline_always auto operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<false>
				(
					NMeta::TCTypeList<>()
					, fg_CurrentActor()
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}

		inline CDispatchHelperWithActor operator () (TCActor<> const &_Actor) const
		{
			return CDispatchHelperWithActor(_Actor);
		}

		inline CDispatchHelperWithWeakActor operator () (TCWeakActor<> const &_Actor) const
		{
			return CDispatchHelperWithWeakActor(_Actor);
		}

		inline CDispatchHelperWithActor operator () (CBlockingActorCheckout const &_Checkout) const
		{
			return CDispatchHelperWithActor(_Checkout.f_Actor());
		}
	};

	extern CDispatchHelper const &g_Dispatch;

	struct [[nodiscard("You need to co_await to yield")]] CCoroutineYieldAwaiter
	{
		bool await_ready() const noexcept
		{
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle)
		{
			auto &ConcurrencyThreadLocal = fg_ConcurrencyThreadLocal();
			auto &ThreadLocal = ConcurrencyThreadLocal.m_SystemThreadLocal;

			DMibFastCheck(fg_CurrentActor(ConcurrencyThreadLocal));

			auto &CoroutineContext = _Handle.promise();
			CoroutineContext.f_Suspend(ThreadLocal, true);

			ConcurrencyThreadLocal.m_pCurrentlyProcessingActorHolder->f_Yield();
			ConcurrencyThreadLocal.m_pCurrentlyProcessingActorHolder->f_QueueProcess
				(
					[
						KeepAlive = CoroutineContext.f_KeepAliveImplicit()
#if DMibEnableSafeCheck > 0
						, pRealActor = ConcurrencyThreadLocal.m_pCurrentlyProcessingActorHolder
#endif
					]
					(CConcurrencyThreadLocal &_ThreadLocal) mutable
					{
						DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == pRealActor);

						if (!KeepAlive.f_HasValidCoroutine())
							return; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

						auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();
						auto &CoroutineContext = Handle.promise();

						{
							bool bAborted = false;
							auto RestoreStates = CoroutineContext.f_Resume(_ThreadLocal.m_SystemThreadLocal, &tf_CCoroutineContext::fs_KeepaliveSetResultFunctor, bAborted);
							if (!bAborted)
								Handle.resume();
						}
					}
					, ConcurrencyThreadLocal
				)
			;

			return true;
		}

		void await_resume() noexcept
		{
		}
	};

	struct [[nodiscard("You need to co_await to yield")]] CCoroutineYield
	{
		auto operator co_await()
		{
			return CCoroutineYieldAwaiter();
		}
	};

	extern CCoroutineYield g_Yield;

	struct CConcurrentDispatchHelper
	{
		template <typename tf_FFunction>
		inline_always auto operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<false>
				(
					NMeta::TCTypeList<>()
					, fg_ConcurrentActor()
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}
	};

	extern CConcurrentDispatchHelper const &g_ConcurrentDispatch;

	struct CDirectDispatchHelper
	{
		template <typename tf_FFunction>
		inline_always auto operator / (tf_FFunction &&_fFunction) const
		{
			return NPrivate::fg_DispatchGenericImpl<true>
				(
					NMeta::TCTypeList<>()
					, fg_CurrentActor()
					, fg_Forward<tf_FFunction>(_fFunction)
				)
			;
		}
	};

	extern CDirectDispatchHelper const &g_DirectDispatch;

	template <typename t_CReturnValue>
	template<typename tf_CFunctor>
	inline_always void TCFuture<t_CReturnValue>::operator > (tf_CFunctor &&_Functor) &&
		requires (NTraits::cIsCallableWith<NTraits::TCRemoveReferenceAndQualifiers<tf_CFunctor>, void (TCAsyncResult<t_CReturnValue> &&)>)
	{
		auto Actor = fg_CurrentActor();
		DMibFastCheck(Actor);
		fg_Move(*this) > fg_Move(Actor) / fg_Forward<tf_CFunctor>(_Functor);
	}

	template <typename t_CReturnValue>
	template <typename tf_CResultActor, typename tf_CResultFunctor>
	void TCFuture<t_CReturnValue>::operator > (TCActorResultCall<tf_CResultActor, tf_CResultFunctor> &&_ResultCall) &&
		requires (NTraits::cIsCallableWith<tf_CResultFunctor, void (TCAsyncResult<t_CReturnValue> &&)>) // Incorrect type in result call
	{
		auto This = fg_Move(*this);

#ifdef DMibCheckOnResultSizes
		if constexpr (sizeof(TCActorResultCall<tf_CResultActor, tf_CResultFunctor>) > gc_FutureOnResultStorage)
		{
			This.f_OnResultSet
				(
					TCFutureOnResultOverSized<t_CReturnValue>
					(
						[ResultCall = fg_Move(_ResultCall)](TCAsyncResult<t_CReturnValue> &&_Result) mutable
						{
							auto Actor = fg_Move(ResultCall.mp_Actor);
							auto pActor = Actor.f_GetRealActor();
							pActor->f_QueueProcess
								(
									[
										Functor = fg_Move(ResultCall.mp_Functor)
										, Result = fg_Move(_Result)
#if DMibEnableSafeCheck > 0
										, pActor
#endif
									]
									(CConcurrencyThreadLocal &_ThreadLocal) mutable
									{
										DMibFastCheck(pActor == _ThreadLocal.m_pCurrentlyProcessingActorHolder);
										NPrivate::fg_CallResultFunctor
											(
												_ThreadLocal
												, Functor
												, fg_Move(Result)
											)
										;
									}
								)
							;
						}
					)
				)
			;
		}
		else
#endif
		{
			This.f_OnResultSet
				(
					[ResultCall = fg_Move(_ResultCall)](TCAsyncResult<t_CReturnValue> &&_Result) mutable
					{
						auto Actor = fg_Move(ResultCall.mp_Actor);
						auto pActor = Actor.f_GetRealActor();
						pActor->f_QueueProcess
							(
								[
									Functor = fg_Move(ResultCall.mp_Functor)
									, Result = fg_Move(_Result)
#if DMibEnableSafeCheck > 0
									, pActor
#endif
								]
								(CConcurrencyThreadLocal &_ThreadLocal) mutable
								{
									DMibFastCheck(pActor == _ThreadLocal.m_pCurrentlyProcessingActorHolder);
									NPrivate::fg_CallResultFunctor
										(
											_ThreadLocal
											, Functor
											, fg_Move(Result)
										)
									;
								}
							)
						;
					}
				)
			;
		}
	}

	template <typename t_CReturnValue>
	template <typename tf_CReturnValue>
	inline_always void TCFuture<t_CReturnValue>::operator > (TCPromise<tf_CReturnValue> &&_Promise) &&
	{
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[Promise = fg_Move(_Promise)](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_CResult>
	inline_always void TCFuture<t_CReturnValue>::operator > (TCPromiseWithExceptionTransfomer<tf_CResult> &&_PromiseWithError) &&
	{
		auto This = fg_Move(*this);
		This.f_OnResultSet
			(
				[Promise = fg_Move(_PromiseWithError.m_Promise), fTransformer = fg_Move(_PromiseWithError).f_GetTransformer()](TCAsyncResult<t_CReturnValue> &&_Result) mutable
				{
					if (!_Result)
						Promise.f_SetException(fTransformer(_Result.f_GetException()));
					else
						Promise.f_SetResult(fg_Move(_Result));
				}
			)
		;
	}

	template <typename t_CReturnValue>
	template <typename tf_CKey, typename tf_CForwardKey>
	void TCFuture<t_CReturnValue>::operator > (TCFutureMapBoundKey<tf_CKey, t_CReturnValue, tf_CForwardKey> &&_BoundKey) &&
	{
		auto This = fg_Move(*this);
		This.f_OnResultSet(_BoundKey.m_This.fp_AddResult(fg_Forward<tf_CForwardKey>(_BoundKey.m_Key)));
	}

	template <typename t_CReturnValue>
	template <typename tf_FOnResult>
	void TCFuture<t_CReturnValue>::operator > (TCDirectResultFunctor<tf_FOnResult> &&_fOnResult) &&
	{
		auto This = fg_Move(*this);
		This.f_OnResultSet(fg_Move(_fOnResult.m_fOnResult));
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::operator > (TCVectorOfFutures<t_CReturnValue> &_ResultVector) &&
	{
		_ResultVector.f_Insert(fg_Move(*this));
	}

	template <typename t_CReturnValue>
	void TCFuture<t_CReturnValue>::operator > (NContainer::TCMapResult<TCFuture<t_CReturnValue> &> &&_MapResult) &&
	{
		*_MapResult = fg_Move(*this);
	}

	template <typename... tp_CFutures>
	template <typename tf_CType>
	inline_always TCFuturePack<tp_CFutures..., TCFuture<tf_CType>> TCFuturePack<tp_CFutures...>::operator + (TCFuture<tf_CType> &&_Future) &&
	{
		return NStorage::fg_TupleConcatenate(fg_Move(m_Futures), NStorage::fg_Tuple(fg_Move(_Future)));
	}

	template <typename... tp_CFutures>
	template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	inline_always auto TCFuturePack<tp_CFutures...>::operator +
		(
			TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other
		) &&
		-> TCFuturePack<tp_CFutures..., TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>
	{
		return NStorage::fg_TupleConcatenate(fg_Move(m_Futures), NStorage::fg_Tuple(fg_Move(_Other)));
	}

	template <typename t_CReturnValue>
	template <typename tf_CType>
	inline_always auto TCFuture<t_CReturnValue>::operator + (TCFuture<tf_CType> &&_Other) &&
	{
		return TCFuturePack<TCFuture, TCFuture<tf_CType>>(NStorage::fg_Tuple(fg_Move(*this), fg_Move(_Other)));
	}

	template <typename t_CReturnValue>
	template <typename tf_CReturn, typename tf_CActor, typename tf_FFunctionPointer, CBindActorOptions tf_BindOptions, bool tf_bByValue, typename ...tfp_CParams>
	auto TCFuture<t_CReturnValue>::operator + (TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...> &&_Other) &&
	{
		return TCFuturePack<TCFuture, TCBoundActorCall<tf_CReturn, tf_CActor, tf_FFunctionPointer, tf_BindOptions, tf_bByValue, tfp_CParams...>>(NStorage::fg_Tuple(fg_Move(*this), fg_Move(_Other)));
	}

	template <typename t_CReturnValue>
	template <typename tf_CReturn, typename tf_CFutureLike>
	inline_always auto TCFuture<t_CReturnValue>::operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other) &&
	{
		return fg_Move(*this) + fg_Move(_Other).f_Dispatch();
	}

	namespace NPrivate
	{
		template <typename t_CType>
		struct TCVectorAllDoneState
		{
			TCVectorAllDoneState(TCPromise<NContainer::TCVector<TCAsyncResult<t_CType>>> &&_Promise)
				: m_Promise(fg_Move(_Promise))
			{
			}

			~TCVectorAllDoneState()
			{
				auto &PromiseData = *m_Promise.f_Unsafe_PromiseData();
				PromiseData.f_OnResult();
			}

			NStorage::CIntrusiveRefCount m_RefCount;
			TCPromise<NContainer::TCVector<TCAsyncResult<t_CType>>> m_Promise;
		};

		template <typename t_CType>
		struct TCVectorAllDoneValueSetter
		{
			void operator ()(TCAsyncResult<t_CType> &&_Result)
			{
				m_Value = fg_Move(_Result);
			}

			TCAsyncResult<t_CType> &m_Value;
			NStorage::TCSharedPointer<TCVectorAllDoneState<t_CType>> m_pState;
		};
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(NContainer::TCVector<TCFuture<tf_CType>> &_Futures)
	{
		if (_Futures.f_IsEmpty())
			return NContainer::TCVector<TCAsyncResult<tf_CType>>();

		TCPromiseFuturePair<NContainer::TCVector<TCAsyncResult<tf_CType>>> Promise;

		NStorage::TCSharedPointer<NPrivate::TCVectorAllDoneState<tf_CType>> pState = fg_Construct(fg_Move(Promise.m_Promise));
		auto &State = *pState;
		auto &PromiseData = *State.m_Promise.f_Unsafe_PromiseData();
		auto &Result = PromiseData.m_Result.f_PrepareResult();
		auto nFutures = _Futures.f_GetLen();
		State.m_RefCount.m_RefCount.f_FetchAdd(nFutures, NAtomic::EMemoryOrder_Relaxed);
		Result.f_SetLen(nFutures);
		auto pResultArray = Result.f_GetArray();

		mint iFuture = 0;
		mint nDirectFutures = 0;
		for (auto &Future : _Futures)
		{
			DMibFastCheck(Future.f_IsValid());
			if (Future.f_OnResultSetNoClear(NPrivate::TCVectorAllDoneValueSetter<tf_CType>{.m_Value = pResultArray[iFuture], .m_pState = fg_Attach(&State)}))
			{
				auto &PromiseData = *Future.f_Unsafe_PromiseData();
				auto *pValueSetter = static_cast<NPrivate::TCVectorAllDoneValueSetter<tf_CType> *>(PromiseData.m_fOnResult.f_Unsafe_GetFunctor());
				pValueSetter->m_pState.f_Detach();
				PromiseData.m_fOnResult.f_Clear();
				++nDirectFutures;
			}

			++iFuture;
		}

		_Futures.f_Clear();

		if (nDirectFutures)
			State.m_RefCount.m_RefCount.f_FetchSub(nDirectFutures, NAtomic::EMemoryOrder_Relaxed);

		return fg_Move(Promise.m_Future);
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<TCAsyncResult<tf_CType>>> fg_AllDoneWrapped(NContainer::TCVector<TCFuture<tf_CType>> &&_Futures)
	{
		return fg_AllDoneWrapped(_Futures);
	}

	namespace NPrivate
	{
		template <typename t_CKey, typename t_CValue>
		struct TCMapAllDoneState
		{
			TCMapAllDoneState(TCPromise<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> &&_Promise)
				: m_Promise(fg_Move(_Promise))
			{
			}

			~TCMapAllDoneState()
			{
				auto &PromiseData = *m_Promise.f_Unsafe_PromiseData();
				PromiseData.f_OnResult();
			}

			NStorage::CIntrusiveRefCount m_RefCount;
			TCPromise<NContainer::TCMap<t_CKey, TCAsyncResult<t_CValue>>> m_Promise;
		};

		template <typename t_CKey, typename t_CValue>
		struct TCMapAllDoneValueSetter
		{
			void operator ()(TCAsyncResult<t_CValue> &&_Result)
			{
				m_Value = fg_Move(_Result);
			}

			TCAsyncResult<t_CValue> &m_Value;
			NStorage::TCSharedPointer<TCMapAllDoneState<t_CKey, t_CValue>> m_pState;
		};
	}

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(NContainer::TCMap<tf_CKey, TCFuture<tf_CValue>> &_Futures)
	{
		if (_Futures.f_IsEmpty())
			return NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>();

		TCPromiseFuturePair<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> Promise;

		NStorage::TCSharedPointer<NPrivate::TCMapAllDoneState<tf_CKey, tf_CValue>> pState = fg_Construct(fg_Move(Promise.m_Promise));
		auto &State = *pState;
		auto &PromiseData = *State.m_Promise.f_Unsafe_PromiseData();
		auto &ResultMap = PromiseData.m_Result.f_PrepareResult();

		mint PrecachedCount = 0;
		mint nDirectFutures = 0;

		for (auto &FutureEntry : _Futures.f_Entries())
		{
			if (PrecachedCount == 0)
			{
				State.m_RefCount.m_RefCount.f_FetchAdd(1024, NAtomic::EMemoryOrder_Relaxed);
				PrecachedCount = 1024;
			}

			--PrecachedCount;
			auto &Future = FutureEntry.f_Value();
			DMibFastCheck(Future.f_IsValid());
			if (Future.f_OnResultSetNoClear(NPrivate::TCMapAllDoneValueSetter<tf_CKey, tf_CValue>{.m_Value = ResultMap[FutureEntry.f_Key()], .m_pState = fg_Attach(&State)}))
			{
				auto &PromiseData = *Future.f_Unsafe_PromiseData();
				auto *pValueSetter = static_cast<NPrivate::TCMapAllDoneValueSetter<tf_CKey, tf_CValue> *>(PromiseData.m_fOnResult.f_Unsafe_GetFunctor());
				pValueSetter->m_pState.f_Detach();
				PromiseData.m_fOnResult.f_Clear();
				++nDirectFutures;
			}
		}

		_Futures.f_Clear();

		State.m_RefCount.m_RefCount.f_FetchSub(PrecachedCount + nDirectFutures, NAtomic::EMemoryOrder_Relaxed);

		return fg_Move(Promise.m_Future);
	}

	template <typename tf_CKey, typename tf_CValue>
	inline_always TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> fg_AllDoneWrapped(NContainer::TCMap<tf_CKey, TCFuture<tf_CValue>> &&_Futures)
	{
		return fg_AllDoneWrapped(_Futures);
	}

	template <typename tf_CType>
	TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(NContainer::TCVector<TCFuture<tf_CType>> &_Futures)
	{
		TCPromiseFuturePair<NContainer::TCVector<tf_CType>> Promise;
		fg_AllDoneWrapped(_Futures).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCVector<TCAsyncResult<tf_CType>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename tf_CType>
	inline_always TCFuture<NContainer::TCVector<tf_CType>> fg_AllDone(NContainer::TCVector<TCFuture<tf_CType>> &&_Futures)
	{
		return fg_AllDone(_Futures);
	}

	template <typename tf_CKey, typename tf_CValue>
	TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(NContainer::TCMap<tf_CKey, TCFuture<tf_CValue>> &_Futures)
	{
		TCPromiseFuturePair<NContainer::TCMap<tf_CKey, tf_CValue>> Promise;
		fg_AllDoneWrapped(_Futures).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<tf_CValue>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename tf_CKey, typename tf_CValue>
	inline_always TCFuture<NContainer::TCMap<tf_CKey, tf_CValue>> fg_AllDone(NContainer::TCMap<tf_CKey, TCFuture<tf_CValue>> &&_Futures)
	{
		return fg_AllDone(_Futures);
	}

	template <typename tf_CKey>
	TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> fg_AllDone(NContainer::TCMap<tf_CKey, TCFuture<void>> &_Futures)
	{
		TCPromiseFuturePair<NContainer::TCSet<tf_CKey>> Promise;
		fg_AllDoneWrapped(_Futures).f_OnResultSet
			(
				[Promise = fg_Move(Promise.m_Promise)](TCAsyncResult<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> &&_Results)
				{
					if (!_Results)
						return Promise.f_SetException(fg_Move(_Results).f_GetException());

					Promise.f_SetResult(fg_Unwrap(fg_Move(*_Results)));
				}
			)
		;

		return fg_Move(Promise.m_Future);
	}

	template <typename tf_CKey>
	inline_always TCFuture<NContainer::TCMap<tf_CKey, TCAsyncResult<void>>> fg_AllDone(NContainer::TCMap<tf_CKey, TCFuture<void>> &&_Futures)
	{
		return fg_AllDone(_Futures);
	}

	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &_Futures);
	TCFuture<void> fg_AllDone(NContainer::TCVector<TCFuture<void>> &&_Futures);
}
