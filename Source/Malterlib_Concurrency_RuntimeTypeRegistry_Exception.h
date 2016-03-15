// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		struct CRuntimeTypeRegistryEntry_Exception
		{
			CRuntimeTypeRegistryEntry_Exception(uint32 _Hash);
			~CRuntimeTypeRegistryEntry_Exception();
					
			virtual std::exception_ptr f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream) pure;
			virtual void f_Feed(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> &_Stream, NException::CExceptionBase const &_Exception) pure;
			
			uint32 m_Hash;
			
			DMibIntrusiveLink(CRuntimeTypeRegistryEntry_Exception, NIntrusive::TCAVLLink<>, m_HashLink);
		};
		
		namespace NPrivate
		{
			template <typename t_CException, uint32 t_NameHash = fg_GetTypeHash<t_CException>()>
			struct TCRuntimeTypeRegistryEntry_Exception final : public CRuntimeTypeRegistryEntry_Exception
			{
				TCRuntimeTypeRegistryEntry_Exception ();

				std::exception_ptr f_Consume(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream) override;
				void f_Feed(NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8>> &_Stream, NException::CExceptionBase const &_Exception) override;
			};
			
			template <typename t_CException>
			struct TCExceptionRegistry
			{
				static TCRuntimeTypeRegistryEntry_Exception<t_CException> ms_Entry;
			};
			
			template <typename tf_CType>
			NContainer::TCVector<uint8> fg_StreamAsyncResultException(NConcurrency::TCAsyncResult<tf_CType> const &_Result);
			
			template <typename tf_CType>
			NContainer::TCVector<uint8> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<tf_CType> const &_Result);
			
			template <>
			inline_always_debug NContainer::TCVector<uint8> fg_StreamAsyncResult(NConcurrency::TCAsyncResult<void> const &_Result);
		}
		
#define DMibConcurrencyRegisterException(d_Exception) (void)::NMib::NConcurrency::NPrivate::TCExceptionRegistry<d_Exception>::ms_Entry
		
	}
}
