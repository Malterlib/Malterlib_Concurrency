// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Storage/Aggregate>

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			template <typename tf_CResult>
			bool fg_CopyReplyToContinuationOrAsyncResultShared(NStream::CBinaryStreamMemoryPtr<> &_Stream, tf_CResult &_ContinuationOrAsyncResult);
		}
		
		template <typename t_CType>
		template <typename tf_CStream>
		void TCAsyncResult<t_CType>::f_Feed(tf_CStream &_Stream) const
		{
			if (*this)
			{
				_Stream << uint8(0); // No exception
				decltype(auto) ToStream = **this;
				_Stream << ToStream;
				return;
			}
			NPrivate::fg_StreamAsyncResultException(_Stream, *this);
		}

		template <typename t_CType>
		template <typename tf_CStream>
		void TCAsyncResult<t_CType>::f_Feed(tf_CStream &_Stream)
		{
			if (*this)
			{
				_Stream << uint8(0); // No exception
				decltype(auto) ToStream = **this;
				_Stream << fg_Move(ToStream);
				return;
			}
			NPrivate::fg_StreamAsyncResultException(_Stream, *this);
		}
		
		template <typename t_CType>
		template <typename tf_CStream>
		void TCAsyncResult<t_CType>::f_Consume(tf_CStream &_Stream)
		{
			if (NPrivate::fg_CopyReplyToContinuationOrAsyncResultShared(_Stream, *this))
				return;
			t_CType Result;
			_Stream >> Result;
			f_SetResult(fg_Move(Result));
		}

		template <typename tf_CStream>
		void TCAsyncResult<void>::f_Feed(tf_CStream &_Stream) const
		{
			if (*this)
			{
				_Stream << uint8(0); // No exception
				return;
			}
			return NPrivate::fg_StreamAsyncResultException(_Stream, *this);
		}
		
		template <typename tf_CStream>
		void TCAsyncResult<void>::f_Consume(tf_CStream &_Stream)
		{
			if (NPrivate::fg_CopyReplyToContinuationOrAsyncResultShared(_Stream, *this))
				return;
			f_SetResult();
		}
		
		namespace NPrivate
		{
			template <typename tf_CStream, typename tf_CType>
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version)
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
				return fg_StreamAsyncResultException(_Result);
			}
			
			template <typename tf_CType>
			inline_always_debug NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> &&_Result, void *_pStreamContext, uint32 _Version)
			{
				return fg_StreamAsyncResult<NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>>(fg_Move(_Result), _pStreamContext, _Version);
			}

			template <typename tf_CStream>
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version)
			{
				if (_Result)
				{
					tf_CStream Stream;
					Stream << uint8(0); // No exception
					return Stream.f_MoveVector();
				}
				return fg_StreamAsyncResultException(_Result);
			}

			inline_always_debug NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> &&_Result, void *_pStreamContext, uint32 _Version)
			{
				return fg_StreamAsyncResult<NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>>(fg_Move(_Result), _pStreamContext, _Version);
			}

			template <typename t_CException, uint32 t_NameHash>
			TCRuntimeTypeRegistryEntry_Exception<t_CException, t_NameHash>::TCRuntimeTypeRegistryEntry_Exception ()
				: CRuntimeTypeRegistryEntry_Exception(t_NameHash)
			{
			}

			template <typename t_CException, uint32 t_NameHash>
			CExceptionPointer TCRuntimeTypeRegistryEntry_Exception<t_CException, t_NameHash>::f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream)
			{
				auto Exception = DMibImpExceptionInstance(t_CException, "");
				_Stream >> Exception;
				
				return fg_ExceptionPointer(fg_Move(Exception));
			}
			
			template <typename t_CException, uint32 t_NameHash>
			void TCRuntimeTypeRegistryEntry_Exception<t_CException, t_NameHash>::f_Feed
				(
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &_Stream
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
				static NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_Exception<t_CException>> ms_Entry;
			};
			
			template <typename t_CException>
			NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_Exception<t_CException>> TCExceptionRegistryImpl<t_CException>::ms_Entry = {DAggregateInit};
			
			template <typename t_CException>
			TCRuntimeTypeRegistryEntry_ExceptionInit<t_CException>::TCRuntimeTypeRegistryEntry_ExceptionInit()
			{
				*TCExceptionRegistryImpl<t_CException>::ms_Entry;
			}
		}
	}
}
