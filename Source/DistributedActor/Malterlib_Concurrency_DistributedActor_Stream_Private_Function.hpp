// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename ...tf_CFunctionOptions>
	void CDistributedActorWriteStream::f_Feed(NFunction::TCFunction<tf_CFunctionOptions...> const &_Function)
	{
		static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;

		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunction<tf_CFunctionOptions...>>>(_Function));
	}

	template <typename tf_CSignature>
	void CDistributedActorWriteStream::f_Feed(NFunction::TCFunctionMutable<tf_CSignature> const &_Function)
	{
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunctionMutable<tf_CSignature>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;

		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunctionMutable<tf_CSignature>>>(_Function));
	}

	template <typename ...tf_CFunctionOptions>
	void CDistributedActorWriteStream::f_Feed(NFunction::TCFunction<tf_CFunctionOptions...> &&_Function)
	{
		static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;

		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunction<tf_CFunctionOptions...>>>(fg_Move(_Function)));
	}

	template <typename tf_CSignature>
	void CDistributedActorWriteStream::f_Feed(NFunction::TCFunctionMovable<tf_CSignature> &&_Function)
	{
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunctionMovable<tf_CSignature>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;

		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunctionMovable<tf_CSignature>>>(fg_Move(_Function)));
	}

	template <typename tf_CSignature>
	void CDistributedActorWriteStream::f_Feed(NFunction::TCFunctionMutable<tf_CSignature> &&_Function)
	{
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunctionMutable<tf_CSignature>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;

		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunctionMutable<tf_CSignature>>>(fg_Move(_Function)));
	}

	namespace NPrivate
	{
		template <typename t_FFunctionSignature>
		struct TCStreamingFunctionHelper;
		
		template <typename t_CReturn, typename ...tp_CParams>
		struct TCStreamingFunctionHelper<TCFuture<t_CReturn> (tp_CParams...)>
		{
			static constexpr mint mc_nParams = sizeof...(tp_CParams);
			
			static auto fs_Functor(NStr::CStr const &_FunctionID)
			{
				return [_FunctionID](tp_CParams ...p_Params) -> TCFuture<t_CReturn>
					{
						auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;

						auto *pActorDataRaw = static_cast<CDistributedActorData *>(ThreadLocal.m_pCurrentRemoteDispatchActorData);

						if (!pActorDataRaw)
							return DMibErrorInstance("This functions needs to be dispatched on a remote actor");
						
						if (!pActorDataRaw->m_bRemote)
							return DMibErrorInstance("Internal error (missing or incorrect distributed actor data)");

						TCPromise<t_CReturn> Promise;
						
						auto pHost = pActorDataRaw->m_pHost.f_Lock();
						if (!pHost || pHost->m_bDeleted)
						{
							Promise.f_SetException(DMibErrorInstance("Remote actor host no longer available"));
							return Promise.f_MoveFuture();
						}
						
						CDistributedActorWriteStream Stream;
						Stream << uint8(0); // Dummy command
						Stream << uint64(0); // Dummy packet ID
						Stream << pActorDataRaw->m_ActorID;				
						Stream << uint32(0);
						Stream << pActorDataRaw->m_ProtocolVersion;
						Stream << _FunctionID;
						
						CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion.f_Load(), true};
						
						TCStreamArguments<NMeta::TCTypeList<tp_CParams...>>::fs_Stream(Stream, Context, pActorDataRaw->m_ProtocolVersion, fg_Forward<tp_CParams>(p_Params)...);
						
						auto pActorData = NStorage::TCSharedPointer<CDistributedActorData>{pActorDataRaw};
						
						TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();
						
						if (!DistributionManager)
						{
							Promise.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
							return Promise.f_MoveFuture();
						}

						DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), Stream.f_MoveVector(), Context)
							> [Promise, Context, Version = pActorDataRaw->m_ProtocolVersion]
							(TCAsyncResult<NContainer::CSecureByteVector> &&_Result) mutable
							{
								if (!_Result)
								{
									Promise.f_SetException(fg_Move(_Result));
									return;
								}
								try
								{
									NException::CDisableExceptionTraceScope DisableTrace;
									fg_CopyReplyToPromise(Promise, *_Result, Context, Version);
								}
								catch (NException::CException const &_Exception)
								{
									Promise.f_SetException(DMibErrorInstance(fg_Format("Exception reading remote result: {}", _Exception.f_GetErrorStr())));
								}								
							}
						;
						return Promise.f_MoveFuture();
					}
				;
			}
			
			template <typename tf_FFunction, mint... tfp_Indices>
			static NConcurrency::TCFuture<NContainer::CSecureByteVector> fs_Call
				(
					CDistributedActorReadStream &_Stream
					, tf_FFunction &_fFunction
					, NMeta::TCIndices<tfp_Indices...> const &_Indices
				)
			{
				NConcurrency::TCPromise<NContainer::CSecureByteVector> Return;

				struct CState
				{
					CDistributedActorStreamContext m_Context;
					NStorage::TCTuple<typename NTraits::TCDecay<tp_CParams>::CType...> m_ParamList;
					uint32 m_Version;
				};
				
				CDistributedActorStreamContext *pContext = (CDistributedActorStreamContext *)_Stream.f_GetContext();
				DMibFastCheck(pContext && pContext->f_CorrectMagic());

				NStorage::TCUniquePointer<CState> pState;

				try
				{
					pState = NStorage::TCUniquePointer<CState>(new CState{*pContext, fg_DecodeParams(_Stream, _Indices, NMeta::TCTypeList<tp_CParams...>()), _Stream.f_GetVersion()});
				}
				catch (NException::CException const &_Exception)
				{
					return Return <<= _Exception;
				}

				auto &State = *pState;

				NFunction::TCFunctionMovable<void (TCAsyncResult<t_CReturn> &&_AsyncResult)> fOnResultSet
					= [Return, pState = fg_Move(pState)](NConcurrency::TCAsyncResult<t_CReturn> &&_Result) mutable
					{
						auto &State = *pState;
						// We need to keep ParamList alive until result is available to make coroutines safe
						Return.f_SetResult(fg_StreamAsyncResult<CDistributedActorWriteStream>(fg_Move(_Result), &State.m_Context, State.m_Version));
					}
				;

				auto &ThreadLocal = **g_SystemThreadLocal;

		#if DMibEnableSafeCheck > 0
				bool bPreviousExpectCoroutineCall = ThreadLocal.m_bExpectCoroutineCall;
				ThreadLocal.m_bExpectCoroutineCall = true;
				auto Cleanup = g_OnScopeExit > [&]
					{
						ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
					}
				;
		#endif

				auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;
				auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
				PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

				auto CleanupOnResultSet = g_OnScopeExit > [&]
					{
						PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
					}
				;

		#if DMibEnableSafeCheck > 0
				auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
				PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

				auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy;
				PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = nullptr;

				auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
				PromiseThreadLocal.m_bCaptureDebugException = true;

				auto CleanupOnResultSetConsumedBy = g_OnScopeExit > [&]
					{
						PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
						PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
						PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					}
				;

				PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<t_CReturn>();

				auto Future = _fFunction(fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(State.m_ParamList))...);

				DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Future.f_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
				DMibFastCheck
					(
					 	ThreadLocal.m_bExpectCoroutineCall
					 	|| !PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy
					 	|| Future.f_HasData(PromiseThreadLocal.m_pExpectCoroutineCallSetConsumedBy)
					)
				;

				ThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
				Cleanup.f_Clear();
		#else
				auto Future = _fFunction(fg_Forward<tp_CParams>(fg_Get<tfp_Indices>(State.m_ParamList))...);
		#endif
				if (PromiseThreadLocal.m_pOnResultSet)
				{
					DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
					Future.f_OnResultSet(fg_Move(fOnResultSet));
				}

				return Return.f_MoveFuture();
			}
		};
	}

	template <typename ...tf_CFunctionOptions>
	void CDistributedActorReadStream::f_Consume(NFunction::TCFunction<tf_CFunctionOptions...> &_Function)
	{
		static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;
		using FFunctionSignature = typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>;

		NStr::CStr FunctionID;
		*this >> FunctionID;
		_Function = NPrivate::TCStreamingFunctionHelper<FFunctionSignature>::fs_Functor(FunctionID);
	}

	template <typename tf_CSignature>
	void CDistributedActorReadStream::f_Consume(NFunction::TCFunctionMovable<tf_CSignature> &_Function)
	{
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunctionMovable<tf_CSignature>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;
		using FFunctionSignature = typename NFunction::TCFunctionInfo<NFunction::TCFunctionMovable<tf_CSignature>>::template TCCallType<0>;

		NStr::CStr FunctionID;
		*this >> FunctionID;
		_Function = NPrivate::TCStreamingFunctionHelper<FFunctionSignature>::fs_Functor(FunctionID);
	}

	template <typename tf_CSignature>
	void CDistributedActorReadStream::f_Consume(NFunction::TCFunctionMutable<tf_CSignature> &_Function)
	{
		static_assert
			(
				NPrivate::TCIsFuture
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunctionMutable<tf_CSignature>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a future are supported"
			)
		;
		using FFunctionSignature = typename NFunction::TCFunctionInfo<NFunction::TCFunctionMutable<tf_CSignature>>::template TCCallType<0>;

		NStr::CStr FunctionID;
		*this >> FunctionID;
		_Function = NPrivate::TCStreamingFunctionHelper<FFunctionSignature>::fs_Functor(FunctionID);
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_FFunction, typename t_FFunctionSignature>
	TCStreamingFunction<t_FFunction, t_FFunctionSignature>::TCStreamingFunction(t_FFunction const &_fFunction)
		: m_fFunction(_fFunction)
	{
	}

	template <typename t_FFunction, typename t_FFunctionSignature>
	TCStreamingFunction<t_FFunction, t_FFunctionSignature>::TCStreamingFunction(t_FFunction &&_fFunction)
		: m_fFunction(fg_Move(_fFunction))
	{
	}
	
	template <typename t_FFunction, typename t_FFunctionSignature>
	auto TCStreamingFunction<t_FFunction, t_FFunctionSignature>::f_Call(CDistributedActorReadStream &_Stream)
		-> NConcurrency::TCFuture<NContainer::CSecureByteVector> 
	{
		return NPrivate::TCStreamingFunctionHelper<t_FFunctionSignature>::fs_Call
			(
				_Stream
				, m_fFunction
				, typename NMeta::TCMakeConsecutiveIndices<NPrivate::TCStreamingFunctionHelper<t_FFunctionSignature>::mc_nParams>::CType()
			)
		;
	}

	template <typename t_FFunction, typename t_FFunctionSignature>
	bool TCStreamingFunction<t_FFunction, t_FFunctionSignature>::f_IsEmpty() const
	{
		return m_fFunction.f_IsEmpty();
	}
	
	template <typename t_FFunction, typename t_FFunctionSignature>
	void const *TCStreamingFunction<t_FFunction, t_FFunctionSignature>::f_GetFunctionPointer() const
	{
		return m_fFunction.f_GetFirstFunctionPointer();
	}
}
