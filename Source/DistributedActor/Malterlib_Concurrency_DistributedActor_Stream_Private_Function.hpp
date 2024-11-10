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
		struct TCStreamingFunctionHelper<t_CReturn (tp_CParams...)>
		{
			static constexpr mint mc_nParams = sizeof...(tp_CParams);
			static constexpr bool mc_bIsFuture = TCIsFuture<t_CReturn>::mc_Value;
			static constexpr bool mc_bIsAsyncGenerator = TCIsAsyncGenerator<t_CReturn>::mc_Value;
			using CReturnType = typename NPrivate::TCGetReturnType<t_CReturn>::CType;
			using CResultType = NConcurrency::TCAsyncResult<CReturnType>;
			using CAsyncGeneratorType = typename TCIsAsyncGenerator<t_CReturn>::CType;

			static_assert(mc_bIsFuture || mc_bIsAsyncGenerator);

			static auto fs_Functor(NStr::CStr const &_FunctionID)
				requires (mc_bIsFuture)
			{
				return [_FunctionID](NTraits::TCDecayType<tp_CParams> ...p_Params) -> t_CReturn
					{
						auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;

						[[maybe_unused]] auto &Coroutine = co_await g_AccessCoroutine;

						auto *pActorDataRaw = static_cast<CDistributedActorData *>(ThreadLocal.m_pCurrentRemoteDispatchActorData);

						if (!pActorDataRaw)
							co_return DMibErrorInstance("This functions needs to be dispatched on a remote actor");

						if (!pActorDataRaw->m_bRemote)
							co_return DMibErrorInstance("Internal error (missing or incorrect distributed actor data)");

						auto pHost = pActorDataRaw->m_pHost.f_Lock();
						if (!pHost || pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
							co_return DMibErrorInstance("Remote actor host no longer available");

						CDistributedActorWriteStream Stream;
						Stream << uint8(0); // Dummy command
						Stream << uint64(0); // Dummy packet ID
						Stream << pActorDataRaw->m_ActorID;
						Stream << uint32(0);
						Stream << pActorDataRaw->m_ProtocolVersion;
						Stream << _FunctionID;

						auto Version = pActorDataRaw->m_ProtocolVersion;

						CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion.f_Load(), true};

						TCStreamArguments<NMeta::TCTypeList<NTraits::TCDecayType<tp_CParams>...>>::fs_Stream(Stream, Context, Version, fg_Move(p_Params)...);

						auto pActorData = NStorage::TCSharedPointer<CDistributedActorData>{pActorDataRaw};

						TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();

						if (!DistributionManager)
							co_return DMibErrorInstance("Actor distribution manager for actor no longer exists");

						auto Result = co_await DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), Stream.f_MoveVector(), Context).f_Wrap();

						if (!Result)
							co_return Result.f_GetException();

						try
						{
							NException::CDisableExceptionTraceScope DisableTrace;
							TCAsyncResult<CReturnType> AsyncResult;
							fg_CopyReplyToAsyncResult(AsyncResult, *Result, Context, Version);

							co_return fg_Move(AsyncResult);
						}
						catch (NException::CException const &)
						{
							co_return DMibErrorInstance(fg_Format("Exception reading remote result: {}", NException::fg_CurrentExceptionString()));
						}
					}
				;
			}

			static auto fs_Functor(NStr::CStr const &_FunctionID)
				requires (mc_bIsAsyncGenerator)
			{
				return [_FunctionID](NTraits::TCDecayType<tp_CParams> ...p_Params) -> t_CReturn
					{
						auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;

						auto *pActorDataRaw = static_cast<CDistributedActorData *>(ThreadLocal.m_pCurrentRemoteDispatchActorData);

						if (!pActorDataRaw)
							co_return DMibErrorInstance("This functions needs to be dispatched on a remote actor");

						if (!pActorDataRaw->m_bRemote)
							co_return DMibErrorInstance("Internal error (missing or incorrect distributed actor data)");

						auto pHost = pActorDataRaw->m_pHost.f_Lock();
						if (!pHost || pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
							co_return DMibErrorInstance("Remote actor host no longer available");

						CDistributedActorWriteStream Stream;
						Stream << uint8(0); // Dummy command
						Stream << uint64(0); // Dummy packet ID
						Stream << pActorDataRaw->m_ActorID;
						Stream << uint32(0);
						Stream << pActorDataRaw->m_ProtocolVersion;
						Stream << _FunctionID;

						CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion.f_Load(), true};

						TCStreamArguments<NMeta::TCTypeList<tp_CParams...>>::fs_Stream(Stream, Context, pActorDataRaw->m_ProtocolVersion, fg_Move(p_Params)...);

						auto pActorData = NStorage::TCSharedPointer<CDistributedActorData>{pActorDataRaw};

						TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();

						if (!DistributionManager)
							co_return DMibErrorInstance("Actor distribution manager for actor no longer exists");

						t_CReturn UpstreamGenerator
							{
								g_ActorFunctor /
								[
									fRemoteFunctor = NStorage::TCOptional<TCActorFunctor<TCFuture<NStorage::TCOptional<CAsyncGeneratorType>> ()>>()
									, Context
									, pActorData = fg_Move(pActorData)
									, Version = pActorDataRaw->m_ProtocolVersion
									, bInProgress = false
									, StreamData = Stream.f_MoveVector()
								]
								() mutable -> TCFuture<NStorage::TCOptional<CAsyncGeneratorType>>
								{
									if (bInProgress)
										co_return DMibErrorInstance("Iteration already in progress");
									bInProgress = true;

									if (!fRemoteFunctor)
									{
										TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();

										if (!DistributionManager)
											co_return DMibErrorInstance("Actor distribution manager for actor no longer exists");

										auto Data = co_await DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), fg_Move(StreamData), Context);

										NException::CDisableExceptionTraceScope DisableTrace;
										{
											auto CaptureScope = co_await (g_CaptureExceptions % "Exception reading remote result");

											CDistributedActorReadStream ReplyStream;
											ReplyStream.f_OpenRead(Data);

											DMibBinaryStreamContext(ReplyStream, &Context);
											DMibBinaryStreamVersion(ReplyStream, Version);

											TCActorFunctorWithID<TCFuture<NStorage::TCOptional<CAsyncGeneratorType>> ()> fRemoteFunctorStream;

											bool bException;
											ReplyStream >> bException;
											if (bException)
												co_return DMibErrorInstance("Unexpected exception in async generator");

											bool bHasGenerator;
											ReplyStream >> bHasGenerator;
											if (bHasGenerator)
											{
												ReplyStream >> fRemoteFunctorStream;
												fRemoteFunctor = fg_Move(fRemoteFunctorStream);
												if (!*fRemoteFunctor)
													co_return DMibErrorInstance("Invalid iteration functor");
											}

											NStr::CStr Error;
											if (!Context.f_ValidateContext(Error))
												co_return DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error));
										}
									}

									auto Result = co_await (*fRemoteFunctor)();

									bInProgress = false;

									co_return fg_Move(Result);
								}
							}
						;

						auto iValue = co_await fg_Move(UpstreamGenerator).f_GetIterator();
						while (iValue)
						{
							co_yield fg_Move(*iValue);
							co_await ++iValue;
						}

						co_return {};
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
				CDistributedActorStreamContext *pContext = (CDistributedActorStreamContext *)_Stream.f_GetContext();
				DMibFastCheck(pContext && pContext->f_CorrectMagic());

				NStorage::TCTuple<typename NTraits::TCDecay<tp_CParams>::CType...> ParamList;
				try
				{
					ParamList = fg_DecodeParams(_Stream, _Indices, NMeta::TCTypeList<tp_CParams...>());
				}
				catch (NException::CException const &_Exception)
				{
					return _Exception;
				}

				auto &ThreadLocal = fg_SystemThreadLocal();
				auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		#if DMibEnableSafeCheck > 0
				bool bPreviousExpectCoroutineCall = PromiseThreadLocal.m_bExpectCoroutineCall;
				PromiseThreadLocal.m_bExpectCoroutineCall = true;

				auto pPreviousExpectCoroutineCallSetConsumedBy = PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy;
				PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = nullptr;

				auto bPreviousCaptureDebugException = PromiseThreadLocal.m_bCaptureDebugException;
				PromiseThreadLocal.m_bCaptureDebugException = true;

				auto Cleanup = g_OnScopeExit / [&]
					{
						PromiseThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
					}
				;

				auto Cleanup2 = g_OnScopeExit / [&]
					{
						PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy = pPreviousExpectCoroutineCallSetConsumedBy;
						PromiseThreadLocal.m_bCaptureDebugException = bPreviousCaptureDebugException;
					}
				;
		#endif
				if constexpr (mc_bIsFuture)
				{
					NConcurrency::TCPromiseFuturePair<NContainer::CSecureByteVector> Return;

					TCFutureOnResult<CReturnType> fOnResultSet =
#if DMibEnableSafeCheck > 0 && defined(DMibCheckOnResultSizes)
						TCFutureOnResultOverSized<CReturnType>
						(
#endif
							[Return = fg_Move(Return.m_Promise), Context = *pContext, Version = _Stream.f_GetVersion()]
							(CResultType &&_Result) mutable
							{
								// We need to keep ParamList alive until result is available to make coroutines safe
								Return.f_SetResult(fg_StreamAsyncResult<CDistributedActorWriteStream>(fg_Move(_Result), &Context, Version));
							}
#if DMibEnableSafeCheck > 0 && defined(DMibCheckOnResultSizes)
						)
#endif
					;

					auto pPreviousOnResultSet = PromiseThreadLocal.m_pOnResultSet;
					PromiseThreadLocal.m_pOnResultSet = &fOnResultSet;

					auto CleanupOnResultSet = g_OnScopeExit / [&]
						{
							PromiseThreadLocal.m_pOnResultSet = pPreviousOnResultSet;
						}
					;

		#if DMibEnableSafeCheck > 0
					auto pPreviousOnResultSetConsumedBy = PromiseThreadLocal.m_pOnResultSetConsumedBy;
					PromiseThreadLocal.m_pOnResultSetConsumedBy = nullptr;

					auto CleanupOnResultSetConsumedBy = g_OnScopeExit / [&]
						{
							PromiseThreadLocal.m_pOnResultSetConsumedBy = pPreviousOnResultSetConsumedBy;
						}
					;

					PromiseThreadLocal.m_OnResultSetTypeHash = fg_GetTypeHash<CReturnType>();

					auto Result = _fFunction(fg_Move(fg_Get<tfp_Indices>(ParamList))...);

					DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet || Result.f_Debug_HasData(PromiseThreadLocal.m_pOnResultSetConsumedBy));
					DMibFastCheck
						(
							PromiseThreadLocal.m_bExpectCoroutineCall
							|| !PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy
							|| Result.f_Debug_HasData(PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy)
						)
					;

					PromiseThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
					Cleanup.f_Clear();
		#else
					auto Result = _fFunction(fg_Move(fg_Get<tfp_Indices>(ParamList))...);
		#endif
					if (PromiseThreadLocal.m_pOnResultSet)
					{
						DMibFastCheck(PromiseThreadLocal.m_pOnResultSet == &fOnResultSet);
						Result.f_OnResultSet(fg_Move(fOnResultSet));
					}

					return fg_Move(Return.m_Future);
				}
				else
				{
		#if DMibEnableSafeCheck > 0
					auto Result = _fFunction(fg_Move(fg_Get<tfp_Indices>(ParamList))...);

					DMibFastCheck
						(
							PromiseThreadLocal.m_bExpectCoroutineCall
							|| !PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy
							|| Result.f_Debug_HasData(PromiseThreadLocal.m_pExpectCoroutineCallConsumedBy)
						)
					;

					PromiseThreadLocal.m_bExpectCoroutineCall = bPreviousExpectCoroutineCall;
					Cleanup.f_Clear();
		#else
					auto Result = _fFunction(fg_Move(fg_Get<tfp_Indices>(ParamList))...);
		#endif
					CDistributedActorWriteStream Stream;
					DMibBinaryStreamContext(Stream, pContext);
					DMibBinaryStreamVersion(Stream, _Stream.f_GetVersion());
					Stream << false; // No exception
					Stream << fg_Move(Result);
					return Stream.f_MoveVector();
				}
			}
		};

		template <typename t_CReturn, typename ...tp_CParams>
		struct TCStreamingFunctionHelper<t_CReturn (tp_CParams...) noexcept> : public TCStreamingFunctionHelper<t_CReturn (tp_CParams...)>
		{
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
	auto TCStreamingFunction<t_FFunction, t_FFunctionSignature>::f_Call(CDistributedActorReadStream &_Stream) -> NConcurrency::TCFuture<NContainer::CSecureByteVector>
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
