// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always NPrivate::CDistributedActorStreamContextState &CDistributedActorWriteStream::f_GetState()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return *pContext->m_pState;
	}

	inline_always NPrivate::CDistributedActorStreamContextState &CDistributedActorReadStream::f_GetState()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return *pContext->m_pState;
	}
	
	inline_always NPtr::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &CDistributedActorWriteStream::f_GetStatePtr()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return pContext->m_pState;
	}

	inline_always NPtr::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &CDistributedActorReadStream::f_GetStatePtr()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return pContext->m_pState;
	}
}

namespace NMib::NConcurrency::NPrivate
{
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
			, CDistributedActorStreamContext &_Context
			, uint32 _Version 
		)
	{
		CDistributedActorReadStream ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToContinuationOrAsyncResultShared(ReplyStream, _Continuation))
			return;
		DMibBinaryStreamContext(ReplyStream, &_Context);
		DMibBinaryStreamVersion(ReplyStream, _Version);
		
		tf_CResult Result;
		ReplyStream >> Result;

		NStr::CStr Error;
		if (!_Context.f_ValidateContext(Error))
		{
			_Continuation.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
			return;
		}
		_Continuation.f_SetResult(fg_Move(Result));
	}

	template <>
	void fg_CopyReplyToContinuation
		(
			TCContinuation<void> &_Continuation
			, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	;

	template <typename t_CTypes>
	struct TCStreamArguments
	{
	};

	template <typename t_CType> 
	constexpr inline_always_debug t_CType&& fg_ForwardToFunctionCall(typename NTraits::TCRemoveReference<t_CType>::CType &_ToForward) 
	{
    	return static_cast<t_CType&&>(_ToForward);
	}

	template <typename t_CType> 
	constexpr inline_always_debug t_CType&& fg_ForwardToFunctionCall(typename NTraits::TCRemoveReference<t_CType>::CType &&_ToForward) noexcept
	{
    	return fg_Move(_ToForward);
	}
	
	template <typename ...tp_CParam>
	struct TCStreamArguments<NMeta::TCTypeList<tp_CParam...>>
	{
		template <typename tf_CStream, typename ...tfp_CParam>
		static auto fs_Stream
			(
				tf_CStream &_Stream
				, CDistributedActorStreamContext &_Context
				, uint32 _Version
				, tfp_CParam && ...p_Params
			)
		{
			DMibBinaryStreamContext(_Stream, &_Context);
			DMibBinaryStreamVersion(_Stream, _Version);
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						_Stream << fg_ForwardToFunctionCall<tp_CParam>(fg_Forward<tfp_CParam>(p_Params)); return true;
					}()...
				}
			;
			(void)Dummy;
		}
	};
	
}

#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Actor.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Function.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Subscription.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_ActorFunctor.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Interface.hpp"
