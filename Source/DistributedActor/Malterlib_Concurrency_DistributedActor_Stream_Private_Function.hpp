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
				NPrivate::TCIsContinuation
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a continuation is supported"
			)
		;
		
		f_FeedFunction(fg_Construct<NPrivate::TCStreamingFunction<NFunction::TCFunction<tf_CFunctionOptions...>>>(_Function));
	}
	
	namespace NPrivate
	{
		template <typename t_FFunctionSignature>
		struct TCStreamingFunctionHelper;
		
		template <typename t_CReturn, typename ...tp_CParams>
		struct TCStreamingFunctionHelper<TCContinuation<t_CReturn> (tp_CParams...)>
		{
			static auto fs_Functor(NStr::CStr const &_FunctionID)
			{
				return [_FunctionID](tp_CParams ...p_Params) -> TCContinuation<t_CReturn>
					{
						auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;

						auto *pActorDataRaw = static_cast<CDistributedActorData *>(ThreadLocal.m_pCurrentRemoteDispatchActorData);

						if (!pActorDataRaw)
							return DMibErrorInstance("This functions needs to be dispatched on a remote actor");
						
						if (!pActorDataRaw->m_bRemote)
							return DMibErrorInstance("Internal error (missing or incorrect distributed actor data)");

						TCContinuation<t_CReturn> Continuation;
						
						auto pHost = pActorDataRaw->m_pHost.f_Lock();
						if (!pHost || pHost->m_bDeleted)
						{
							Continuation.f_SetException(DMibErrorInstance("Remote actor host no longer available"));
							return Continuation;
						}
						
						CDistributedActorWriteStream Stream;
						Stream << uint8(0); // Dummy command
						Stream << uint64(0); // Dummy packet ID
						Stream << pActorDataRaw->m_ActorID;				
						Stream << 0;
						Stream << pActorDataRaw->m_ProtocolVersion;
						Stream << _FunctionID;
						
						CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion, true};
						
						TCStreamArguments<NMeta::TCTypeList<tp_CParams...>>::fs_Stream(Stream, Context, pActorDataRaw->m_ProtocolVersion, fg_Forward<tp_CParams>(p_Params)...);
						
						auto pActorData = NPtr::TCSharedPointer<CDistributedActorData>{pActorDataRaw};
						
						TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();
						
						if (!DistributionManager)
						{
							Continuation.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
							return Continuation;
						}

						DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), Stream.f_MoveVector(), Context)
							> [Continuation, Context, Version = pActorDataRaw->m_ProtocolVersion](TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result) mutable
							{
								if (!_Result)
								{
									Continuation.f_SetException(fg_Move(_Result));
									return;
								}
								try
								{
									NException::CDisableExceptionTraceScope DisableTrace;
									fg_CopyReplyToContinuation(Continuation, *_Result, Context, Version);
								}
								catch (NException::CException const &_Exception)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("Exception reading remote result: {}", _Exception.f_GetErrorStr())));
								}								
							}
						;
						return Continuation;
					}
				;
			}
		};
	}

	template <typename ...tf_CFunctionOptions>
	void CDistributedActorReadStream::f_Consume(NFunction::TCFunction<tf_CFunctionOptions...> &_Function)
	{
		static_assert(NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::mc_nCalls == 1, "Only supports functions with one signature");
		static_assert
			(
				NPrivate::TCIsContinuation
				<
					typename NTraits::TCFunctionTraits<typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>>::CReturn
				>::mc_Value
				, "Only functions that return a continuation is supported"
			)
		;
		using FFunctionSignature = typename NFunction::TCFunctionInfo<NFunction::TCFunction<tf_CFunctionOptions...>>::template TCCallType<0>;

		NStr::CStr FunctionID;
		*this >> FunctionID;
		_Function = NPrivate::TCStreamingFunctionHelper<FFunctionSignature>::fs_Functor(FunctionID);
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::TCStreamingFunction(t_FFunction const &_fFunction)
		: m_fFunction(_fFunction)
	{
	}

	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::TCStreamingFunction(t_FFunction &&_fFunction)
		: m_fFunction(fg_Move(_fFunction))
	{
	}
	
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	template <mint... tfp_Indices>
	NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::fp_Call
		(
			CDistributedActorReadStream &_Stream
			, NMeta::TCIndices<tfp_Indices...> const &_Indices
		)
	{
		NContainer::TCTuple<typename NTraits::TCDecay<tp_CParams>::CType...> ParamList;
		CDistributedActorStreamContext *pContext = (CDistributedActorStreamContext *)_Stream.f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		
		try
		{
			ParamList = fg_DecodeParams(_Stream, _Indices, NMeta::TCTypeList<tp_CParams...>());
		}
		catch (NException::CException const &_Exception)
		{
			return _Exception;
		}
		
		auto Continuation = m_fFunction(fg_Forward<tp_CParams>(NContainer::fg_Get<tfp_Indices>(ParamList))...);
	
		NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Return;
		
		Continuation.f_OnResultSet
			(
				[Return, Context = *pContext, Version = _Stream.f_GetVersion()](NConcurrency::TCAsyncResult<t_CReturn> &&_Result) mutable
				{
					Return.f_SetResult(fg_StreamAsyncResult<CDistributedActorWriteStream>(fg_Move(_Result), &Context, Version));
				}
			)
		;
		
		return Return;
	}
	
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	auto TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::f_Call(CDistributedActorReadStream &_Stream)
		-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
	{
		return fp_Call(_Stream, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tp_CParams)>::CType());
	}
}
