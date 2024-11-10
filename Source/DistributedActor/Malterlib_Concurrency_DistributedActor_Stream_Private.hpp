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
	
	inline_always NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &CDistributedActorWriteStream::f_GetStatePtr()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return pContext->m_pState;
	}

	inline_always NStorage::TCSharedPointer<NPrivate::CDistributedActorStreamContextState> const &CDistributedActorReadStream::f_GetStatePtr()
	{
		auto *pContext = (NPrivate::CDistributedActorStreamContext *)f_GetContext();
		DMibFastCheck(pContext && pContext->f_CorrectMagic());
		return pContext->m_pState;
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CResult>
	bool fg_CopyReplyToAsyncResultShared(NStream::CBinaryStreamMemoryPtr<> &_Stream, TCAsyncResult<tf_CResult> &_PromiseOrAsyncResult, uint32 _ActorProtocolVersion)
	{
		uint8 bException;
		_Stream >> bException;
		
		if (bException)
		{
			uint32 TypeHash;
			_Stream >> TypeHash;

			uint64 ExceptionStreamSize = 0;
			if (_ActorProtocolVersion >= NConcurrency::EDistributedActorProtocolVersion_GeneralExceptionsSupported)
				NStream::fg_ConsumeLenFromStream(_Stream, ExceptionStreamSize);
			else if (TypeHash == 0x2d572b80u)
				TypeHash = NException::CException::ms_TypeHash;

			auto &TypeRegistry = fg_RuntimeTypeRegistry();
			auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);

			if (!pEntry)
			{
				_Stream.f_AddPosition(ExceptionStreamSize);
				_PromiseOrAsyncResult.f_SetException(DMibErrorInstance("Unknown exception type received"));
				return true;
			}
			
			_PromiseOrAsyncResult.f_SetException(pEntry->f_Consume(_Stream));
			
			return true;
		}
		return false;
	}
	
	template <typename tf_CResult>
	void fg_CopyReplyToAsyncResult
		(
			TCAsyncResult<tf_CResult> &_AsyncResult
			, NContainer::CSecureByteVector const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	{
		CDistributedActorReadStream ReplyStream;
		ReplyStream.f_OpenRead(_Data);
		if (fg_CopyReplyToAsyncResultShared(ReplyStream, _AsyncResult, _Context.f_ActorProtocolVersion()))
			return;
		DMibBinaryStreamContext(ReplyStream, &_Context);
		DMibBinaryStreamVersion(ReplyStream, _Version);
		
		tf_CResult Result;
		ReplyStream >> Result;

		NStr::CStr Error;
		if (!_Context.f_ValidateContext(Error))
		{
			_AsyncResult.f_SetException(DMibErrorInstance(fg_Format("Invalid set of parameter and return types: {}", Error)));
			return;
		}
		_AsyncResult.f_SetResult(fg_Move(Result));
	}

	template <>
	void fg_CopyReplyToAsyncResult
		(
			TCAsyncResult<void> &_Promise
			, NContainer::CSecureByteVector const &_Data
			, CDistributedActorStreamContext &_Context
			, uint32 _Version
		)
	;

	template <typename t_CTypes>
	struct TCStreamArguments
	{
	};

	template 
	<
		typename t_CType
		, typename t_CTypeFrom
		, bool t_bIsSameType = NTraits::TCIsSame<typename NTraits::TCRemoveReference<t_CType>::CType, typename NTraits::TCRemoveReference<t_CTypeFrom>::CType>::mc_Value
	>
	struct TCStreamForwardHelper
	{
		template <typename tf_CType> 
		static constexpr inline_always_debug typename NTraits::TCRemoveReferenceAndQualifiers<t_CType>::CType fs_Forward(tf_CType &&_Value) noexcept
		{
			return fg_Forward<tf_CType>(_Value);
		}
	};

	template <typename t_CType, typename t_CTypeFrom>
	struct TCStreamForwardHelper<t_CType &&, t_CTypeFrom, true>
	{
		template <typename tf_CType> 
		static constexpr inline_always_debug t_CType &&fs_Forward(tf_CType &&_Value) noexcept
		{
			return fg_Move(fg_Forward<tf_CType>(_Value));
		}
	};

	template <typename t_CType, typename t_CTypeFrom>
	struct TCStreamForwardHelper<t_CType &, t_CTypeFrom, true>
	{
		template <typename tf_CType> 
		static constexpr inline_always_debug t_CType &&fs_Forward(tf_CType &&_Value) noexcept
		{
			return fg_Move(fg_Forward<tf_CType>(_Value));
		}
	};

	template <typename t_CType, typename t_CTypeFrom>
	struct TCStreamForwardHelper<t_CType const &, t_CTypeFrom, true>
	{
		template <typename tf_CType> 
		static constexpr inline_always_debug t_CType const &fs_Forward(tf_CType &&_Value) noexcept
		{
			return fg_Forward<tf_CType>(_Value);
		}
	};
	
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
			(
				[&]
				{
					_Stream << TCStreamForwardHelper<tp_CParam, tfp_CParam>::fs_Forward(fg_Forward<tfp_CParam>(p_Params));
					return true;
				}
				()
				, ...
			);
		}
	};
	
}

#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Actor.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Function.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Subscription.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_ActorFunctor.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_Interface.hpp"
#include "Malterlib_Concurrency_DistributedActor_Stream_Private_AsyncResult.hpp"
