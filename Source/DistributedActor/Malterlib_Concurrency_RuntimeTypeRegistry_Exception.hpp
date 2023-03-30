// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Aggregate>
namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CResult>
	bool fg_CopyReplyToPromiseOrAsyncResultShared(NStream::CBinaryStreamMemoryPtr<> &_Stream, tf_CResult &_PromiseOrAsyncResult, uint32 _ActorProtocolVersion);
}

namespace NMib::NConcurrency
{
	template <typename t_CType>
	template <typename tf_CStream>
	void TCAsyncResult<t_CType>::f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const
	{
		if (*this)
		{
			_Stream << uint8(0); // No exception
			decltype(auto) ToStream = **this;
			_Stream << ToStream;
			return;
		}
		NPrivate::fg_StreamAsyncResultException(_Stream, *this, _ActorProtocolVersion);
	}

	template <typename t_CType>
	template <typename tf_CStream>
	void TCAsyncResult<t_CType>::f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion)
	{
		if (*this)
		{
			_Stream << uint8(0); // No exception
			decltype(auto) ToStream = **this;
			_Stream << fg_Move(ToStream);
			return;
		}
		NPrivate::fg_StreamAsyncResultException(_Stream, *this, _ActorProtocolVersion);
	}

	template <typename t_CType>
	template <typename tf_CStream>
	void TCAsyncResult<t_CType>::f_ConsumeWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion)
	{
		if (NPrivate::fg_CopyReplyToPromiseOrAsyncResultShared(_Stream, *this, _ActorProtocolVersion))
			return;
		t_CType Result;
		_Stream >> Result;
		f_SetResult(fg_Move(Result));
	}

	template <typename tf_CStream>
	void TCAsyncResult<void>::f_FeedWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion) const
	{
		if (*this)
		{
			_Stream << uint8(0); // No exception
			return;
		}
		return NPrivate::fg_StreamAsyncResultException(_Stream, *this, _ActorProtocolVersion);
	}

	template <typename tf_CStream>
	void TCAsyncResult<void>::f_ConsumeWithProtocol(tf_CStream &_Stream, uint32 _ActorProtocolVersion)
	{
		if (NPrivate::fg_CopyReplyToPromiseOrAsyncResultShared(_Stream, *this, _ActorProtocolVersion))
			return;
		f_SetResult();
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename tf_CStream, typename tf_CType>
	NContainer::CSecureByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version)
	{
		if (_Result)
		{
			tf_CStream Stream;
			DMibBinaryStreamContext(Stream, _pStreamContext);
			DMibBinaryStreamVersion(Stream, _Version);
			Stream << uint8(0); // No exception
			Stream << fg_Move(*_Result);
			return Stream.f_MoveVector();
		}

		return fg_StreamAsyncResultException(_Result, fg_ActorProtocolVersion(_pStreamContext));
	}

	template <typename tf_CType>
	inline_always_debug NContainer::CSecureByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version)
	{
		return fg_StreamAsyncResult<NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector>>(fg_Move(_Result), _pStreamContext, _Version);
	}

	template <typename tf_CStream>
	NContainer::CSecureByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version)
	{
		if (_Result)
		{
			tf_CStream Stream;
			Stream << uint8(0); // No exception
			return Stream.f_MoveVector();
		}

		return fg_StreamAsyncResultException(_Result, fg_ActorProtocolVersion(_pStreamContext));
	}

	inline_always_debug NContainer::CSecureByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version)
	{
		return fg_StreamAsyncResult<NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector>>(fg_Move(_Result), _pStreamContext, _Version);
	}

	template <typename t_CException>
	TCRuntimeTypeRegistryEntry_Exception<t_CException>::TCRuntimeTypeRegistryEntry_Exception ()
		: CRuntimeTypeRegistryEntry_Exception(c_NameHash)
	{
	}

	template <typename t_CException>
	NException::CExceptionPointer TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream)
	{
		auto Exception = DMibImpExceptionInstance(t_CException, "");
		_Stream >> Exception;

		return NException::fg_MakeException(fg_Move(Exception));
	}

	template <typename t_CException>
	NException::CExceptionPointer TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Consume(NStream::CBinaryStreamDefault &_Stream)
	{
		auto Exception = DMibImpExceptionInstance(t_CException, "");
		_Stream >> Exception;

		return NException::fg_MakeException(fg_Move(Exception));
	}

	template <typename t_CException>
	NException::CExceptionPointer TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Consume(NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream)
	{
		auto Exception = DMibImpExceptionInstance(t_CException, "");
		_Stream >> Exception;

		return NException::fg_MakeException(fg_Move(Exception));
	}

	template <typename t_CException>
	void TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Feed
		(
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> &_Stream
			, NException::CExceptionBase const &_Exception
		)
	{
		auto &Exception = static_cast<t_CException const &>(_Exception);
		_Stream << Exception;
	}

	template <typename t_CException>
	void TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Feed
		(
			NStream::CBinaryStreamDefault &_Stream
			, NException::CExceptionBase const &_Exception
		)
	{
		auto &Exception = static_cast<t_CException const &>(_Exception);
		_Stream << Exception;
	}

	template <typename t_CException>
	void TCRuntimeTypeRegistryEntry_Exception<t_CException>::f_Feed
		(
			NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream
			, NException::CExceptionBase const &_Exception
		)
	{
		auto &Exception = static_cast<t_CException const &>(_Exception);
		_Stream << Exception;
	}

	template <typename t_CException>
	TCRuntimeTypeRegistryEntry_ExceptionInit<t_CException> TCExceptionRegistry<t_CException>::ms_EntryInit;

	template <typename t_CException>
	struct TCExceptionRegistryImpl
	{
		static NStorage::TCAggregate<TCRuntimeTypeRegistryEntry_Exception<t_CException>> ms_Entry;
	};

	template <typename t_CException>
	constinit NStorage::TCAggregate<TCRuntimeTypeRegistryEntry_Exception<t_CException>> TCExceptionRegistryImpl<t_CException>::ms_Entry = {DAggregateInit};

	template <typename t_CException>
	TCRuntimeTypeRegistryEntry_ExceptionInit<t_CException>::TCRuntimeTypeRegistryEntry_ExceptionInit()
	{
		*TCExceptionRegistryImpl<t_CException>::ms_Entry;
	}
}
