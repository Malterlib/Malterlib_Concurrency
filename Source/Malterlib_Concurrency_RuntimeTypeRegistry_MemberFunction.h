// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		struct CRuntimeTypeRegistry_MemberFunctionPointer
		{
			enum
			{
				EMaxPtrs = 2
			};
			void *m_pPtrs[EMaxPtrs];
			
			inline_always_debug CRuntimeTypeRegistry_MemberFunctionPointer(void **_pPtrs, mint _nPtrs);
			inline_always_debug bool operator < (CRuntimeTypeRegistry_MemberFunctionPointer const &_Other) const;
		};
		
		struct CRuntimeTypeRegistryEntry_MemberFunction
		{
			CRuntimeTypeRegistryEntry_MemberFunction(uint32 _Hash, uint32 _TypeHash, CRuntimeTypeRegistry_MemberFunctionPointer const &_MemberPointer);
			~CRuntimeTypeRegistryEntry_MemberFunction();
			
			virtual NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
					, void *_pObject
				)
				pure
			;
			
			uint32 m_Hash;
			uint32 m_TypeHash;
			CRuntimeTypeRegistry_MemberFunctionPointer m_MemberPointer;
			
			DMibIntrusiveLink(CRuntimeTypeRegistryEntry_MemberFunction, NIntrusive::TCAVLLink<>, m_HashLink);
			DMibIntrusiveLink(CRuntimeTypeRegistryEntry_MemberFunction, NIntrusive::TCAVLLink<>, m_MemberPointerLink);
		};
		
		namespace NPrivate
		{

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction>
			struct TCStaticMemberPointer
			{
				static t_CMemberFunction const mc_Value;
			};

			template 
			<
				typename t_CMemberFunction
				, t_CMemberFunction t_pMemberFunction
				, uint32 t_NameHash
				, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CReturn
			>
			struct TCRuntimeTypeRegistryEntry_MemberFunction final : public CRuntimeTypeRegistryEntry_MemberFunction
			{
				static_assert(sizeof(t_pMemberFunction) <= sizeof(CRuntimeTypeRegistry_MemberFunctionPointer), "Only simple member function pointers are supported");
				
				TCRuntimeTypeRegistryEntry_MemberFunction();
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
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

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			struct TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void> final : public CRuntimeTypeRegistryEntry_MemberFunction
			{
				static_assert(sizeof(t_pMemberFunction) <= sizeof(CRuntimeTypeRegistry_MemberFunctionPointer), "Only simple member function pointers are supported");
				
				TCRuntimeTypeRegistryEntry_MemberFunction();
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
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
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CResult>
			struct TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>> final : public CRuntimeTypeRegistryEntry_MemberFunction
			{
				static_assert(sizeof(t_pMemberFunction) <= sizeof(CRuntimeTypeRegistry_MemberFunctionPointer), "Only simple member function pointers are supported");
				
				TCRuntimeTypeRegistryEntry_MemberFunction();
				
				template <mint... tfp_Indices, typename... tfp_CParams>
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> fp_Call
					(
						NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
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
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			struct TCMemberFunctionRegistry
			{
				static TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash> ms_Entry;
			};
		}
		
#define DMibConcurrencyRegisterMemberFunction(d_CMemberFunction, d_pMemberFunction, d_NameHash) (void)NPrivate::TCMemberFunctionRegistry<d_CMemberFunction, d_pMemberFunction, d_NameHash>::ms_Entry
	}
}
