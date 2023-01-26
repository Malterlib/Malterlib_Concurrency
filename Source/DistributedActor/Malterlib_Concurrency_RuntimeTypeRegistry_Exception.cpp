// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Core/Core>
#include <Mib/Exception/Exception>

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.h"

namespace NMib::NConcurrency::NPrivate
{
	void fg_StreamAsyncResultException
		(
		 	NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> &_Stream
		 	, NException::CExceptionBase const &_Exception
		 	, uint32 _ActorProtocolVersion
		)
	{
		_Stream << uint8(1); // Exception

		uint32 TypeHash = _Exception.f_TypeHash();
		DMibFastCheck(TypeHash);
		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);

		DMibFastCheck(pEntry);
		if (_ActorProtocolVersion < 0x106 && TypeHash == NException::CException::ms_TypeHash)
			TypeHash = 0x2d572b80u;

		_Stream << TypeHash;

		if (_ActorProtocolVersion >= 0x106)
		{
			NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> MeasureSizeStream;
			pEntry->f_Feed(MeasureSizeStream, _Exception);
			uint64 Size = MeasureSizeStream.f_GetLength();
			NStream::fg_FeedLenToStream(_Stream, Size);
		}

		pEntry->f_Feed(_Stream, _Exception);
	}

	void fg_StreamAsyncResultException
		(
		 	NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> &_Stream
		 	, NConcurrency::CAsyncResult const &_Result
		 	, uint32 _ActorProtocolVersion
		)
	{
		bool bHandled = NException::fg_VisitException<NException::CExceptionBase>
			(
				_Result.f_GetException()
				, [&](NException::CExceptionBase const &_Exception)
				{
					fg_StreamAsyncResultException(_Stream, _Exception, _ActorProtocolVersion);
				}
			)
		;

		if (bHandled)
			return;

		return fg_StreamAsyncResultException(_Stream, DMibErrorInstance("Non Malterlib exception encountered in remote actor call"), _ActorProtocolVersion);
	}

	NContainer::CSecureByteVector fg_StreamAsyncResultException(NException::CExceptionBase const &_Exception, uint32 _ActorProtocolVersion)
	{
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		fg_StreamAsyncResultException(Stream, _Exception, _ActorProtocolVersion);
		return Stream.f_MoveVector();
	}

	NContainer::CSecureByteVector fg_StreamAsyncResultException(NConcurrency::CAsyncResult const &_Result, uint32 _ActorProtocolVersion)
	{
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CSecureByteVector> Stream;
		fg_StreamAsyncResultException(Stream, _Result, _ActorProtocolVersion);
		return Stream.f_MoveVector();
	}

	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionBase const &_Exception)
	{
		uint32 TypeHash = _Exception.f_TypeHash();
		DMibFastCheck(TypeHash);
		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);

		DMibFastCheck(pEntry);

		_Stream << TypeHash;
		{
			NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> MeasureSizeStream;
			pEntry->f_Feed(MeasureSizeStream, _Exception);
			uint64 Size = MeasureSizeStream.f_GetLength();
			NStream::fg_FeedLenToStream(_Stream, Size);
		}

		pEntry->f_Feed(_Stream, _Exception);
	}

	NException::CExceptionPointer fg_ConsumeException(NStream::CBinaryStreamDefault &_Stream)
	{
		uint32 TypeHash;
		_Stream >> TypeHash;

		uint64 ExceptionStreamSize = 0;
		NStream::fg_ConsumeLenFromStream(_Stream, ExceptionStreamSize);

		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
		if (pEntry)
			return pEntry->f_Consume(_Stream);
		else
		{
			_Stream.f_AddPosition(ExceptionStreamSize);
			return DMibErrorInstance("Unknown exception type received").f_ExceptionPointer();
		}
	}

	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionPointer const &_pException)
	{
		NException::CDisableExceptionTraceScope DisableTrace;
		try
		{
			std::rethrow_exception(_pException);
		}
		catch (NException::CException const &_Exception)
		{
			return fg_FeedException(_Stream, _Exception);
		}
	}
}
