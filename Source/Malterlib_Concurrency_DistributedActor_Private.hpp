// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CFirst>
	void fg_GetDistributedActorInheritanceHierarchy(NContainer::TCVector<uint32> &_Settings)
	{
		_Settings.f_Insert(fg_GetTypeHash<tf_CFirst>());
		fg_GetDistributedActorInheritanceHierarchy<typename NTraits::TCGetBase<tf_CFirst>::CType>(_Settings);
	}
	
	template <>
	inline_always_debug void fg_GetDistributedActorInheritanceHierarchy<CActor>(NContainer::TCVector<uint32> &_Settings)
	{
	}

	struct CRemoteDispatchActor;
	
	struct CSubSystem_Concurrency_DistributedActor : public CSubSystem
	{
		CSubSystem_Concurrency_DistributedActor();
		~CSubSystem_Concurrency_DistributedActor();

		struct CThreadLocal
		{
			CCallingHostInfo m_CallingHostInfo;
			CDistributedActorData *m_pCurrentRemoteDispatchActorData = nullptr;
		};
		
		NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
	};
	
	CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem();

	template <typename tf_CResult>
	bool fg_CopyReplyToContinuationOrAsyncResultShared(NStream::CBinaryStreamMemoryPtr<> &_Stream, tf_CResult &_ContinuationOrAsyncResult)
	{
		uint8 bException;
		_Stream >> bException;
		
		if (bException)
		{
			uint32 TypeHash;
			_Stream >> TypeHash;
			
			auto &TypeRegistry = fg_RuntimeTypeRegistry();
			auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
			
			if (!pEntry)
			{
				_ContinuationOrAsyncResult.f_SetException(DMibErrorInstance("Unknown exception type received"));
				return true;
			}
			
			_ContinuationOrAsyncResult.f_SetException(pEntry->f_Consume(_Stream));
			
			return true;
		}
		return false;
	}
	
	template <typename tf_CResult>
	void fg_CopyReplyToContinuation
		(
			TCContinuation<tf_CResult> &_Continuation
			, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data
			, CDistributedActorStreamContextSending &_Context
			, uint32 _Version 
		)
	{
		NStream::CBinaryStreamMemoryPtr<> ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToContinuationOrAsyncResultShared(ReplyStream, _Continuation))
			return;
		DMibBinaryStreamContext(ReplyStream, &_Context);
		DMibBinaryStreamVersion(ReplyStream, _Version);
		
		tf_CResult Result;
		decltype(auto) ToStream = _Context.f_GetValueForConsume(Result);
		ReplyStream >> ToStream;

		if (!_Context.f_ValueReceived())
		{
			_Continuation.f_SetException(DMibErrorInstance("Invalid set of parameter and return types"));
			return;
		}
		_Continuation.f_SetResult(fg_Move(Result));
	}

	template <>
	void fg_CopyReplyToContinuation
		(
			TCContinuation<void> &_Continuation
			, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data
			, CDistributedActorStreamContextSending &_Context
			, uint32 _Version
		)
	;

	template <typename t_CTypes>
	struct TCStreamArguments
	{
		
	};
	
	template <typename ...tp_CParam>
	struct TCStreamArguments<NMeta::TCTypeList<tp_CParam...>>
	{
		template <typename tf_CStream>
		static void fs_Stream
			(
				tf_CStream &_Stream
				, CDistributedActorStreamContextSending &_Context
				, uint32 _Version
				, typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tp_CParam>::CType>::CType const &...p_Params
			)
		{
			DMibBinaryStreamContext(_Stream, &_Context);
			DMibBinaryStreamVersion(_Stream, _Version);
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						_Stream << _Context.f_GetValueForFeed(p_Params); return true;
					}()...
				}
			;
			(void)Dummy;
		}
	};
	
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	TCStreamFunctionReceive<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::TCStreamFunctionReceive(t_FFunction &_fFunction, CDistributedActorStreamContextReceiveState &_State)
		: m_fFunction(_fFunction)
		, m_State(_State) 
	{
	}
	
	template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
	TCStreamingFunction<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::TCStreamingFunction(t_FFunction const &_fFunction)
		: m_fFunction(_fFunction)
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
		CDistributedActorStreamContextReceiving *pContext = (CDistributedActorStreamContextReceiving *)_Stream.f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		
		try
		{
			ParamList = fg_DecodeParams<CDistributedActorStreamContextReceiving>(_Stream, _Indices, NMeta::TCTypeList<tp_CParams...>());
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
					Return.f_SetResult(fg_StreamAsyncResult(_Result, Context, Version));
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
