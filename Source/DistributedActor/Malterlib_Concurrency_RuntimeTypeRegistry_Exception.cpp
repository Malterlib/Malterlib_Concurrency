// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include <Mib/Core/Core>
#include <Mib/Exception/Exception>

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.h"

#include "Malterlib_Concurrency_DistributedActor.h"

namespace NMib::NConcurrency::NPrivate
{
	void fg_StreamAsyncResultException
		(
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream
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
		if (_ActorProtocolVersion < EDistributedActorProtocolVersion_GeneralExceptionsSupported && TypeHash == NException::CException::ms_TypeHash)
			TypeHash = 0x2d572b80u;

		_Stream << TypeHash;

		if (_ActorProtocolVersion >= EDistributedActorProtocolVersion_GeneralExceptionsSupported)
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
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream
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

	NContainer::CIOByteVector fg_StreamAsyncResultException(NException::CExceptionBase const &_Exception, uint32 _ActorProtocolVersion)
	{
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
		fg_StreamAsyncResultException(Stream, _Exception, _ActorProtocolVersion);
		return Stream.f_MoveVector();
	}

	NContainer::CIOByteVector fg_StreamAsyncResultException(NConcurrency::CAsyncResult const &_Result, uint32 _ActorProtocolVersion)
	{
		NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> Stream;
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

	NException::CExceptionPointer fg_ConsumeUnknownException(NStream::CBinaryStreamDefault &_Stream, uint32 _TypeHash, uint64 _ExceptionStreamSize)
	{
		NStream::CFilePos EndPosition = _Stream.f_GetPosition() + NStream::CFilePos(_ExceptionStreamSize);

		// Every exception type feeds the base-exception error string first (CExceptionBase::f_Feed), so decode that
		// prefix as a generic exception to keep the original message; the concrete type's extra payload is skipped.
		// The recovered message notes that it did not arrive as its correct exception type.
		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pBaseEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(NException::CException::ms_TypeHash);
		if (pBaseEntry)
		{
			try
			{
				NException::CExceptionPointer pException = pBaseEntry->f_Consume(_Stream);
				if (_Stream.f_GetPosition() <= EndPosition)
				{
					_Stream.f_SetPosition(EndPosition);
					return DMibErrorInstance
						(
							NStr::fg_Format
								(
									"{} (received as unregistered exception type {nh})"
									, NException::fg_ExceptionString(pException)
									, _TypeHash
								)
						).f_ExceptionPointer()
					;
				}
			}
			catch (...)
			{
			}
		}

		_Stream.f_SetPosition(EndPosition);
		return DMibErrorInstance(NStr::fg_Format("Unknown exception type received (type hash {nh})", _TypeHash)).f_ExceptionPointer();
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

		return fg_ConsumeUnknownException(_Stream, TypeHash, ExceptionStreamSize);
	}

	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionPointer const &_pException)
	{
		NException::CDisableExceptionTraceScope DisableTrace;
		if
			(
				!NException::fg_VisitException<NException::CExceptionBase>
				(
					_pException
					, [&](NException::CExceptionBase const &_Exception)
					{
						fg_FeedException(_Stream, _Exception);
					}
				)
			)
		{
			// Unknow exception cannot be streamed
			DMibPDebugBreak;
		}
	}
}
