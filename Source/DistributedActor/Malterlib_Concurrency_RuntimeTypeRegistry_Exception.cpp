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
	void fg_StreamAsyncResultException(NStream::TCBinaryStreamStorage<NStream::CBinaryStreamDefault> &_Stream, NException::CExceptionBase const &_Exception, uint32 _ActorProtocolVersion)
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

		// A peer from before exception stream versions reads the layout of version 0
		uint32 StreamVersion = 0;
		if (_ActorProtocolVersion >= EDistributedActorProtocolVersion_ExceptionStreamVersions)
		{
			StreamVersion = pEntry->f_StreamVersion();
			_Stream << StreamVersion;
		}

		if (_ActorProtocolVersion >= EDistributedActorProtocolVersion_GeneralExceptionsSupported)
		{
			NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> MeasureSizeStream;
			DMibBinaryStreamVersion(MeasureSizeStream, StreamVersion);
			pEntry->f_Feed(MeasureSizeStream, _Exception);
			uint64 Size = MeasureSizeStream.f_GetLength();
			NStream::fg_FeedLenToStream(_Stream, Size);
		}

		DMibBinaryStreamVersion(_Stream, StreamVersion);
		pEntry->f_Feed(_Stream, _Exception);
	}

	void fg_StreamAsyncResultException(NStream::TCBinaryStreamStorage<NStream::CBinaryStreamDefault> &_Stream, NConcurrency::CAsyncResult const &_Result, uint32 _ActorProtocolVersion)
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

	NStream::CBinaryStorage fg_StreamAsyncResultException(NException::CExceptionBase const &_Exception, uint32 _ActorProtocolVersion)
	{
		NStream::TCBinaryStreamStorage<NStream::CBinaryStreamDefault> Stream;
		fg_StreamAsyncResultException(Stream, _Exception, _ActorProtocolVersion);
		return Stream.f_MoveStorage();
	}

	NStream::CBinaryStorage fg_StreamAsyncResultException(NConcurrency::CAsyncResult const &_Result, uint32 _ActorProtocolVersion)
	{
		NStream::TCBinaryStreamStorage<NStream::CBinaryStreamDefault> Stream;
		fg_StreamAsyncResultException(Stream, _Result, _ActorProtocolVersion);
		return Stream.f_MoveStorage();
	}

	/// Feeds an exception with or without its stream version. Feed it versioned only to a reader that has exception stream versions,
	/// such as data whose own stream version says so, and consume it with the same _bVersioned. Versioned it has the layout of
	/// actor replies; unversioned it streams as version 0, which leaves out newer fields.
	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionBase const &_Exception, bool _bVersioned)
	{
		uint32 TypeHash = _Exception.f_TypeHash();
		DMibFastCheck(TypeHash);
		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);

		DMibFastCheck(pEntry);

		_Stream << TypeHash;

		uint32 StreamVersion = 0;
		if (_bVersioned)
		{
			StreamVersion = pEntry->f_StreamVersion();
			_Stream << StreamVersion;
		}

		{
			NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> MeasureSizeStream;
			DMibBinaryStreamVersion(MeasureSizeStream, StreamVersion);
			pEntry->f_Feed(MeasureSizeStream, _Exception);
			NStream::fg_FeedLenToStream(_Stream, MeasureSizeStream.f_GetLength());
		}

		DMibBinaryStreamVersion(_Stream, StreamVersion);
		pEntry->f_Feed(_Stream, _Exception);
	}

	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionPointer const &_pException, bool _bVersioned)
	{
		NException::CDisableExceptionTraceScope DisableTrace;
		if
			(
				!NException::fg_VisitException<NException::CExceptionBase>
				(
					_pException
					, [&](NException::CExceptionBase const &_Exception)
					{
						fg_FeedException(_Stream, _Exception, _bVersioned);
					}
				)
			)
		{
			// Unknow exception cannot be streamed
			DMibPDebugBreak;
		}
	}

	// Recovers the base message for an unregistered type, or returns a generic error if decoding fails.
	// Both results include the type hash and leave the stream positioned after the sized payload.
	NException::CExceptionPointer fg_ConsumeUnknownException(NStream::CBinaryStreamDefault &_Stream, uint32 _TypeHash, uint64 _ExceptionStreamSize)
	{
		NStream::CFilePos EndPosition = _Stream.f_GetPosition() + NStream::CFilePos(_ExceptionStreamSize);

		// CExceptionBase::f_Feed writes the error string before the concrete exception payload.
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

	/// Consumes what fg_FeedException fed with the same _bVersioned, reads it with the sender's stream version, and continues after the
	/// payload whatever this version read of it
	NException::CExceptionPointer fg_ConsumeException(NStream::CBinaryStreamDefault &_Stream, bool _bVersioned)
	{
		uint32 TypeHash;
		_Stream >> TypeHash;

		uint32 StreamVersion = 0;
		if (_bVersioned)
			_Stream >> StreamVersion;

		uint64 ExceptionStreamSize = 0;
		NStream::fg_ConsumeLenFromStream(_Stream, ExceptionStreamSize);

		auto &TypeRegistry = fg_RuntimeTypeRegistry();
		auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
		if (!pEntry)
			return fg_ConsumeUnknownException(_Stream, TypeHash, ExceptionStreamSize);

		NStream::CFilePos EndPosition = _Stream.f_GetPosition() + NStream::CFilePos(ExceptionStreamSize);

		NException::CExceptionPointer pException;
		{
			DMibBinaryStreamVersion(_Stream, StreamVersion);
			pException = pEntry->f_Consume(_Stream);
		}

		// Seeking back would decode what follows from bytes the exception already used
		if (_Stream.f_GetPosition() > EndPosition)
			DMibError(NStr::fg_Format("The exception (type hash {nh}) is longer than the size it was sent with", TypeHash));

		_Stream.f_SetPosition(EndPosition);
		return pException;
	}
}
