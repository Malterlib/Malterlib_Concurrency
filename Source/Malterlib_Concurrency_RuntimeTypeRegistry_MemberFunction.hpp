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
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CReturn>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CReturn>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
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
				
				NContainer::TCTuple<typename NTraits::TCRemoveQualifiers<typename NTraits::TCRemoveReference<tfp_CParams>::CType>::CType...> ParamList;
				
				TCInitializerList<bool> Dummy = 
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
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CReturn>::f_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
					, void *_pObject
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
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
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void>::fp_Call
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
				
				TCInitializerList<bool> Dummy = 
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
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, void>::f_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
					, void *_pObject
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
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
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CResult>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_CParams...> const &
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
				
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							_ParamsStream >> NContainer::fg_Get<tfp_Indices>(ParamList);
							return true;
						}()...
					}
				;
				(void)Dummy;
				
				auto Continuation = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indices>(ParamList))...);
			
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
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, NConcurrency::TCContinuation<t_CResult>>::f_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_Stream
					, void *_pObject
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
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
			TCRuntimeTypeRegistryEntry_MemberFunctionInit<t_CMemberFunction, t_pMemberFunction, t_NameHash> 
				TCMemberFunctionRegistry<t_CMemberFunction, t_pMemberFunction, t_NameHash>::ms_EntryInit
			;
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			struct TCMemberFunctionRegistryImpl
			{
				static NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash>> ms_Entry;
			};

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash>> 
				TCMemberFunctionRegistryImpl<t_CMemberFunction, t_pMemberFunction, t_NameHash>::ms_Entry = {DAggregateInit}
			;
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash>
			TCRuntimeTypeRegistryEntry_MemberFunctionInit<t_CMemberFunction, t_pMemberFunction, t_NameHash>::TCRuntimeTypeRegistryEntry_MemberFunctionInit()
			{
				*TCMemberFunctionRegistryImpl<t_CMemberFunction, t_pMemberFunction, t_NameHash>::ms_Entry;
			}
		}
	}
}
