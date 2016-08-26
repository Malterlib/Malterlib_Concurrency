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
			template <typename tf_CType>
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> const &_Result)
			{
				if (_Result)
				{
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
					Stream << uint8(0); // No exception
					Stream << *_Result;
					return Stream.f_MoveVector();
				}
				return fg_StreamAsyncResultException(_Result);
			}

			template <>
			inline_always_debug NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> const &_Result)
			{
				if (_Result)
				{
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
					Stream << uint8(0); // No exception
					return Stream.f_MoveVector();
				}
				return fg_StreamAsyncResultException(_Result);
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
