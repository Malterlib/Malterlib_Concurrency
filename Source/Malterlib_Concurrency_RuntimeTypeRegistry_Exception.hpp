// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			template <typename tf_CType>
			NContainer::TCVector<uint8> fg_StreamAsyncResultException(NConcurrency::TCAsyncResult<tf_CType> const &_Result)
			{
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> Stream;
				Stream << uint8(1); // Exception
				
				uint32 TypeHash = 0;
				try
				{
					*_Result;
					DMibFastCheck(false); // Should never get here
				}
				catch (NException::CExceptionBase const &_Exception)
				{
					TypeHash = _Exception.f_TypeHash();
					DMibFastCheck(TypeHash);
					auto &TypeRegistry = fg_RuntimeTypeRegistry();
					auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
					
					DMibFastCheck(pEntry);
					
					Stream << TypeHash;
					
					pEntry->f_Feed(Stream, _Exception);
				}
				
				return Stream.f_MoveVector();
			}

			template <typename tf_CType>
			NContainer::TCVector<uint8> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> const &_Result)
			{
				if (_Result)
				{
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> Stream;
					Stream << uint8(0); // No exception
					Stream << *_Result;
					return Stream.f_MoveVector();
				}
				return fg_StreamAsyncResultException(_Result);
			}

			template <>
			inline_always_debug NContainer::TCVector<uint8> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> const &_Result)
			{
				if (_Result)
				{
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> Stream;
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
			std::exception_ptr TCRuntimeTypeRegistryEntry_Exception<t_CException, t_NameHash>::f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream)
			{
				auto Exception = DMibImpExceptionInstance(t_CException, "");
				_Stream >> Exception;
				
				return std::make_exception_ptr(fg_Move(Exception));
			}
			
			template <typename t_CException, uint32 t_NameHash>
			void TCRuntimeTypeRegistryEntry_Exception<t_CException, t_NameHash>::f_Feed(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> &_Stream, NException::CExceptionBase const &_Exception)
			{
				auto &Exception = static_cast<t_CException const &>(_Exception);
				_Stream << Exception;
			}			

			template <typename t_CException>
			TCRuntimeTypeRegistryEntry_Exception<t_CException> TCExceptionRegistry<t_CException>::ms_Entry;
		}
	}
}
