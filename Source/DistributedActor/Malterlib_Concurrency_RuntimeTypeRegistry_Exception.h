// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Memory/Allocators/Secure>
#include <Mib/Intrusive/AVLTree>

namespace NMib::NException
{
	using CExceptionPointer = std::exception_ptr;
}

namespace NMib::NConcurrency
{
	struct CRuntimeTypeRegistryEntry_Exception
	{
		CRuntimeTypeRegistryEntry_Exception(uint32 _Hash);
		~CRuntimeTypeRegistryEntry_Exception();

		virtual NException::CExceptionPointer f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream) = 0;
		virtual NException::CExceptionPointer f_Consume(NStream::CBinaryStreamDefault &_Stream) = 0;
		virtual NException::CExceptionPointer f_Consume(NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream) = 0;

		virtual void f_Feed(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream, NException::CExceptionBase const &_Exception) = 0;
		virtual void f_Feed(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionBase const &_Exception) = 0;
		virtual void f_Feed(NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream, NException::CExceptionBase const &_Exception) = 0;

		uint32 m_Hash;

		NIntrusive::TCAVLLink<> m_HashLink;
	};
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename t_CException>
	struct TCRuntimeTypeRegistryEntry_Exception final : public CRuntimeTypeRegistryEntry_Exception
	{
		TCRuntimeTypeRegistryEntry_Exception ();

		NException::CExceptionPointer f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream) override;
		NException::CExceptionPointer f_Consume(NStream::CBinaryStreamDefault &_Stream) override;
		NException::CExceptionPointer f_Consume(NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream) override;

		void f_Feed(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream, NException::CExceptionBase const &_Exception) override;
		void f_Feed(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionBase const &_Exception) override;
		void f_Feed(NStream::TCBinaryStreamNull<NStream::CBinaryStreamDefault> &_Stream, NException::CExceptionBase const &_Exception) override;

		static constexpr uint32 c_NameHash = DMibConstantTypeHash(t_CException);
	};

	template <typename t_CException>
	struct TCRuntimeTypeRegistryEntry_ExceptionInit
	{
		TCRuntimeTypeRegistryEntry_ExceptionInit();
	};

	template <typename t_CException>
	struct TCExceptionRegistry
	{
		static TCRuntimeTypeRegistryEntry_ExceptionInit<t_CException> ms_EntryInit;
	};

	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionPointer const &_pException);
	void fg_FeedException(NStream::CBinaryStreamDefault &_Stream, NException::CExceptionBase const &_pException);
	NException::CExceptionPointer fg_ConsumeException(NStream::CBinaryStreamDefault &_Stream);

	// Consumes an exception payload whose type hash is not registered in this binary. The base-exception prefix (the
	// error string every exception type feeds first) is decoded as a generic exception so the original message
	// survives — annotated with the unregistered type hash so the mismatch stays visible — and the rest of the
	// payload is skipped. Falls back to a generic error naming the type hash when the payload cannot be decoded that
	// way. The stream is left positioned directly after the payload either way.
	NException::CExceptionPointer fg_ConsumeUnknownException(NStream::CBinaryStreamDefault &_Stream, uint32 _TypeHash, uint64 _ExceptionStreamSize);

	void fg_StreamAsyncResultException
		(
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream
			, NException::CExceptionBase const &_Exception
			, uint32 _ActorProtocolVersion
		)
	;
	void fg_StreamAsyncResultException
		(
			NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::CIOByteVector> &_Stream
			, NConcurrency::CAsyncResult const &_Result
			, uint32 _ActorProtocolVersion
		)
	;

	NContainer::CIOByteVector fg_StreamAsyncResultException(NException::CExceptionBase const &_Exception, uint32 _ActorProtocolVersion);
	NContainer::CIOByteVector fg_StreamAsyncResultException(NConcurrency::CAsyncResult const &_Result, uint32 _ActorProtocolVersion);

	uint32 fg_ActorProtocolVersion(void *_pStreamContext);

	template <typename tf_CStream, typename tf_CType>
	NContainer::CIOByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version);
	template <typename tf_CType>
	inline_always_debug NContainer::CIOByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version);
	template <typename tf_CStream>
	NContainer::CIOByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version);
	inline_always_debug NContainer::CIOByteVector fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version);

#define DMibConcurrencyRegisterException(d_Exception) (void)::NMib::NConcurrency::NPrivate::TCExceptionRegistry<d_Exception>::ms_EntryInit

}
