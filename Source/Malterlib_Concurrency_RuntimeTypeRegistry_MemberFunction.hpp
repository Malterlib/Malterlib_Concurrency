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
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CReturn>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, t_CReturn>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
				)
			{
			}
			
			template <typename tf_CStreamContext, mint... tfp_Indices, typename... tfp_CParams>
			auto fg_DecodeParams
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, NMeta::TCIndices<tfp_Indices...> const &
					, NMeta::TCTypeList<tfp_CParams...> const &
				)
			{
				NContainer::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
				tf_CStreamContext *pContext = (tf_CStreamContext *)_ParamsStream.f_GetContext();
				TCInitializerList<bool> Dummy = 
					{
						[&]
						{
							decltype(auto) Param = pContext->f_GetValueForConsume(NContainer::fg_Get<tfp_Indices>(ParamList));
							_ParamsStream >> Param;
							return true;
						}
						()...
					}
				;
				(void)Dummy;
				return ParamList;
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CReturn>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, t_CReturn>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &_Indices
					, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
				
				try
				{
					ParamList = fg_DecodeParams<t_CStreamContext>(_ParamsStream, _Indices, _TypeList);
				}
				catch (NException::CException const &_Exception)
				{
					return _Exception;
				}
				
				auto Result = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indices>(ParamList))...);
			
				NConcurrency::TCAsyncResult<t_CReturn> AsyncResult;
				AsyncResult.f_SetResult(fg_Move(Result));
				
				t_CStreamContext *pContext = (t_CStreamContext *)_ParamsStream.f_GetContext();
				return NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>::fs_Finished(fg_StreamAsyncResult(AsyncResult, *pContext));
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CReturn>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, t_CReturn>::f_Call
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
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, void>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, void>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &_Indices
					, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
				t_CStreamContext *pContext = (t_CStreamContext *)_ParamsStream.f_GetContext();

				try
				{
					ParamList = fg_DecodeParams<t_CStreamContext>(_ParamsStream, _Indices, _TypeList);
				}
				catch (NException::CException const &_Exception)
				{
					return _Exception;
				}
				
				(((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indices>(ParamList))...);
				
				NConcurrency::TCAsyncResult<void> AsyncResult;
				AsyncResult.f_SetResult();
				
				return NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>>::fs_Finished(fg_StreamAsyncResult(AsyncResult, *pContext));
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, void>::f_Call
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
			
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CResult>
			TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, NConcurrency::TCContinuation<t_CResult>>::TCRuntimeTypeRegistryEntry_MemberFunction()
				: CRuntimeTypeRegistryEntry_MemberFunction
				(
					t_NameHash
					, fg_GetTypeHash<typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass>() 
				)
			{
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CResult>
			template <mint... tfp_Indices, typename... tfp_CParams>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, NConcurrency::TCContinuation<t_CResult>>::fp_Call
				(
					NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamDefault> &_ParamsStream
					, void *_pObject
					, NMeta::TCIndices<tfp_Indices...> const &_Indices
					, NMeta::TCTypeList<tfp_CParams...> const &_TypeList
				)
				-> NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> 
			{
				using CClass = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CClass;
				
				NContainer::TCTuple<typename NTraits::TCDecay<tfp_CParams>::CType...> ParamList;
				t_CStreamContext *pContext = (t_CStreamContext *)_ParamsStream.f_GetContext();
				
				try
				{
					ParamList = fg_DecodeParams<t_CStreamContext>(_ParamsStream, _Indices, _TypeList);
				}
				catch (NException::CException const &_Exception)
				{
					return _Exception;
				}
				
				auto Continuation = (((CClass *)_pObject)->*t_pMemberFunction)(fg_Forward<tfp_CParams>(NContainer::fg_Get<tfp_Indices>(ParamList))...);
			
				NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Return;
				
				Continuation.f_OnResultSet
					(
						[Return, Context = *pContext](NConcurrency::TCAsyncResult<t_CResult> &&_Result) mutable
						{
							Return.f_SetResult(fg_StreamAsyncResult(_Result, Context));
						}
					)
				;
				
				return Return;
			}

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext, typename t_CResult>
			auto TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext, NConcurrency::TCContinuation<t_CResult>>::f_Call
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

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			TCRuntimeTypeRegistryEntry_MemberFunctionInit<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext> 
				TCMemberFunctionRegistry<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>::ms_EntryInit
			;
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			struct TCMemberFunctionRegistryImpl
			{
				static NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>> ms_Entry;
			};

			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			NAggregate::TCAggregate<TCRuntimeTypeRegistryEntry_MemberFunction<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>> 
				TCMemberFunctionRegistryImpl<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>::ms_Entry = {DAggregateInit}
			;
			
			template <typename t_CMemberFunction, t_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename t_CStreamContext>
			TCRuntimeTypeRegistryEntry_MemberFunctionInit<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>::TCRuntimeTypeRegistryEntry_MemberFunctionInit()
			{
				*TCMemberFunctionRegistryImpl<t_CMemberFunction, t_pMemberFunction, t_NameHash, t_CStreamContext>::ms_Entry;
			}
		}
	}
}
