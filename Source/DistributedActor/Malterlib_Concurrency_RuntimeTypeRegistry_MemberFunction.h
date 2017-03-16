// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		struct CRuntimeTypeRegistryEntry_MemberFunction
		{
			CRuntimeTypeRegistryEntry_MemberFunction
				(
					uint32 _Hash
					, uint32 _TypeHash
				)
			;
			~CRuntimeTypeRegistryEntry_MemberFunction();
			
			virtual NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
					, void *_pObject
				)
				= 0
			;
			
			uint32 m_Hash;
			uint32 m_TypeHash;
			
			DMibIntrusiveLink(CRuntimeTypeRegistryEntry_MemberFunction, NIntrusive::TCAVLLink<>, m_HashLink);
			DMibIntrusiveLink(CRuntimeTypeRegistryEntry_MemberFunction, NIntrusive::TCAVLLink<>, m_MemberPointerLink);
		};
		
		namespace NPrivate
		{
			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CStreamContext
				, typename t_CStreamParams
				, typename t_CStreamResult
				, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CReturn
			>
			struct TCRuntimeTypeRegistryEntry_MemberFunction final : public CRuntimeTypeRegistryEntry_MemberFunction
			{
				TCRuntimeTypeRegistryEntry_MemberFunction();
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						t_CStreamParams &_ParamsStream
						, void *_pObject
						, NMeta::TCIndices<tfp_Indices...> const &
						, NMeta::TCTypeList<tfp_CParams...> const &
					)
				;
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
						, void *_pObject
					)
					override
				;
			};

			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CStreamContext
				, typename t_CStreamParams
				, typename t_CStreamResult
			>
			struct TCRuntimeTypeRegistryEntry_MemberFunction
				<
					t_CMemberFunction
					, t_pMemberFunction
					, t_NameHash
					, t_CStreamContext
					, t_CStreamParams
					, t_CStreamResult
					, void
				> final 
				: public CRuntimeTypeRegistryEntry_MemberFunction
			{
				TCRuntimeTypeRegistryEntry_MemberFunction();
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						t_CStreamParams &_ParamsStream
						, void *_pObject
						, NMeta::TCIndices<tfp_Indices...> const &
						, NMeta::TCTypeList<tfp_CParams...> const &
					)
				;

				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
						, void *_pObject
					)
					override
				;
			};
			
			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CStreamContext
				, typename t_CStreamParams
				, typename t_CStreamResult
				, typename t_CResult
			>
			struct TCRuntimeTypeRegistryEntry_MemberFunction
				<
					t_CMemberFunction
					, t_pMemberFunction
					, t_NameHash
					, t_CStreamContext
					, t_CStreamParams
					, t_CStreamResult
					, NConcurrency::TCContinuation<t_CResult>
				> final 
				: public CRuntimeTypeRegistryEntry_MemberFunction
			{
				TCRuntimeTypeRegistryEntry_MemberFunction();
				
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						t_CStreamParams &_ParamsStream
						, void *_pObject
						, NMeta::TCIndices<tfp_Indices...> const &
						, NMeta::TCTypeList<tfp_CParams...> const &
					)
				;

				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
						, void *_pObject
					)
					override
				;
			};

			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CStreamContext
				, typename t_CStreamParams
				, typename t_CStreamResult
			>
			struct TCRuntimeTypeRegistryEntry_MemberFunctionInit
			{
				TCRuntimeTypeRegistryEntry_MemberFunctionInit();
			};

			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CStreamContext = zuint32
				, typename t_CStreamParams = NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault>
				, typename t_CStreamResult = NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>
			>
			struct TCMemberFunctionRegistry
			{
				static TCRuntimeTypeRegistryEntry_MemberFunctionInit
					<
						t_CMemberFunction
						, t_pMemberFunction
						, t_NameHash
						, t_CStreamContext
						, t_CStreamParams
						, t_CStreamResult
					>
					ms_EntryInit
				;
			};
		}
		
#define DMibConcurrencyRegisterMemberFunction(d_CMemberFunction, d_pMemberFunction, d_NameHash) \
			(void)::NMib::NConcurrency::NPrivate::TCMemberFunctionRegistry<d_CMemberFunction, d_pMemberFunction, d_NameHash>::ms_EntryInit

#define DMibConcurrencyRegisterMemberFunctionWithStreams(d_CMemberFunction, d_pMemberFunction, d_NameHash, d_StreamContext, d_StreamParams, d_StreamResult) \
			(void)::NMib::NConcurrency::NPrivate::TCMemberFunctionRegistry<d_CMemberFunction, d_pMemberFunction, d_NameHash, d_StreamContext, d_StreamParams, d_StreamResult>::ms_EntryInit
	}
}
