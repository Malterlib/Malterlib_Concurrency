// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		CRuntimeTypeRegistry_MemberFunctionPointer::CRuntimeTypeRegistry_MemberFunctionPointer(void **_pPtrs, mint _nPtrs)
		{
			mint iPtr = 0;
			for (; iPtr < _nPtrs; ++iPtr)
				m_pPtrs[iPtr] = _pPtrs[_nPtrs];
			for (; iPtr < EMaxPtrs; ++iPtr)
				m_pPtrs[iPtr] = nullptr;
		}
			
		bool CRuntimeTypeRegistry_MemberFunctionPointer::operator < (CRuntimeTypeRegistry_MemberFunctionPointer const &_Other) const
		{
			for (mint iPtr = 0; iPtr < EMaxPtrs; ++iPtr)
			{
				if (m_pPtrs[iPtr] < _Other.m_pPtrs[iPtr])
					return true;
				else if (m_pPtrs[iPtr] > _Other.m_pPtrs[iPtr])
					return false;
			}
			
			return false;
		}
		
		namespace NPrivate
		{
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction>
			t_CMemberFunction const TCStaticMemberPointer<t_CMemberFunction, t_pMemberFunction>::mc_Value = t_pMemberFunction;

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CReturn>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CReturn>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
					, reinterpret_cast<CRuntimeTypeRegistry_MemberFunctionPointer const &>(TCStaticMemberPointer<t_CMemberFunction, t_pMemberFunction>::mc_Value)
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CReturn>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CReturn>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_CParams...> const &
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<tfp_CParams...> ParamList;
				
				std::initializer_list<bool> Dummy = 
					{
						[&]
						{
							_ParamsStream >> NContainer::fg_Get<tfp_Indices>(ParamList);
							return true;
						}()...
					}
				;
				(void)Dummy;
				
				auto Result = (((CClass *)_pObject)->*t_pMemberFunction)(NContainer::fg_Get<tfp_Indices>(ParamList)...);
			
				NConcurrency::TCAsyncResult<t_CReturn> AsyncResult;
				AsyncResult.f_SetResult(fg_Move(Result));
				
				return NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>::fs_Finished(fg_StreamAsyncResult(AsyncResult));
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CReturn>
			NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CReturn>
				::f_Call(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject)
			{
				using CParams = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CParams;
				return fp_Call
					(
						_Stream
						, _pObject
						,
	#ifndef DCompiler_MSVC
						typename 
	#endif
						NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
						, CParams()
					)
				;
			}
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
					, reinterpret_cast<CRuntimeTypeRegistry_MemberFunctionPointer const &>(TCStaticMemberPointer<t_CMemberFunction, t_pMemberFunction>::mc_Value)
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			template <mint... tfp_Indices, typename... tfp_CParams>
			NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_CParams...> const &
				)
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<tfp_CParams...> ParamList;
				
				std::initializer_list<bool> Dummy = 
					{
						[&]
						{
							_ParamsStream >> NContainer::fg_Get<tfp_Indices>(ParamList);
							return true;
						}()...
					}
				;
				(void)Dummy;
				
				(((CClass *)_pObject)->*t_pMemberFunction)(NContainer::fg_Get<tfp_Indices>(ParamList)...);
				
				NConcurrency::TCAsyncResult<void> AsyncResult;
				AsyncResult.f_SetResult();
				
				return NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>::fs_Finished(fg_StreamAsyncResult(AsyncResult));
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void>::f_Call(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject)
			{
				using CParams = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CParams;
				return fp_Call
					(
						_Stream
						, _pObject
						,
	#ifndef DCompiler_MSVC
						typename 
	#endif
						NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
						, CParams()
					)
				;
			}
			
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CResult>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
					, reinterpret_cast<CRuntimeTypeRegistry_MemberFunctionPointer const &>(TCStaticMemberPointer<t_CMemberFunction, t_pMemberFunction>::mc_Value)
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CResult>
			template <mint... tfp_Indices, typename... tfp_CParams>
			NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_CParams...> const &
				)
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<tfp_CParams...> ParamList;
				
				std::initializer_list<bool> Dummy = 
					{
						[&]
						{
							_ParamsStream >> NContainer::fg_Get<tfp_Indices>(ParamList);
							return true;
						}()...
					}
				;
				(void)Dummy;
				
				auto Continuation = (((CClass *)_pObject)->*t_pMemberFunction)(NContainer::fg_Get<tfp_Indices>(ParamList)...);
			
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Return;
				
				Continuation.f_OnResultSet
					(
						[Return](NConcurrency::TCAsyncResult<t_CResult> &&_Result)
						{
							Return.f_SetResult(fg_StreamAsyncResult(_Result));
						}
					)
				;
				
				return Return;
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CResult>
			NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>>::f_Call(NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream, void *_pObject)
			{
				using CParams = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CParams;
				return fp_Call
					(
						_Stream
						, _pObject
						,
	#ifndef DCompiler_MSVC
						typename 
	#endif
						NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<CParams>::mc_Value>::CType()
						, CParams()
					)
				;
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash> TCMemberFunctionRegistry<t_CMemberFunction, t_pMemberFunction, t_NameHash>::ms_Entry;
		}
	}
}
