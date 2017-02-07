// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#define DMibRuntimeTypeRegistry

#include <Mib/Core/Core>
#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "Malterlib_Concurrency_RuntimeTypeRegistry_Exception.h"

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResultException(NException::CExceptionBase const &_Exception)
			{
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
				Stream << uint8(1); // Exception
				
				uint32 TypeHash = _Exception.f_TypeHash();
				DMibFastCheck(TypeHash);
				auto &TypeRegistry = fg_RuntimeTypeRegistry();
				auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
				
				DMibFastCheck(pEntry);
				
				Stream << TypeHash;
				
				pEntry->f_Feed(Stream, _Exception);
				
				return Stream.f_MoveVector();
			}
		
			NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> fg_StreamAsyncResultException(NConcurrency::CAsyncResult const &_Result)
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				try
				{
					_Result.f_Access();
					DMibFastCheck(false); // Should never get here
				}
				catch (NException::CExceptionBase const &_Exception)
				{
					return fg_StreamAsyncResultException(_Exception);
				}
				catch (...)
				{
				}
				
				return fg_StreamAsyncResultException(DMibErrorInstance("Non Malterlib exception encountered in remote actor call"));
			}

			void fg_StreamAsyncResultException(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &_Stream, NException::CExceptionBase const &_Exception)
			{
				_Stream << uint8(1); // Exception
				
				uint32 TypeHash = _Exception.f_TypeHash();
				DMibFastCheck(TypeHash);
				auto &TypeRegistry = fg_RuntimeTypeRegistry();
				auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
				
				DMibFastCheck(pEntry);
				
				_Stream << TypeHash;
				
				pEntry->f_Feed(_Stream, _Exception);
			}
			
			void fg_StreamAsyncResultException(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &_Stream, NConcurrency::CAsyncResult const &_Result)
			{
				NException::CDisableExceptionTraceScope DisableTrace;
				try
				{
					_Result.f_Access();
					DMibFastCheck(false); // Should never get here
				}
				catch (NException::CExceptionBase const &_Exception)
				{
					return fg_StreamAsyncResultException(_Stream, _Exception);
				}
				catch (...)
				{
				}
				
				return fg_StreamAsyncResultException(_Stream, DMibErrorInstance("Non Malterlib exception encountered in remote actor call"));
			}
			
		}
	}
}
