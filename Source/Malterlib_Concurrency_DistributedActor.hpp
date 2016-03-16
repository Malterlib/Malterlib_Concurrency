// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "Malterlib_Concurrency_Manager.h"

namespace NMib
{
	namespace NPtr
	{
		namespace NPrivate
		{
			template <typename t_CActor0, typename t_CActor1, typename t_CAllocator0, typename t_CAllocator1>
			struct TCIsValidConversion<NConcurrency::TCDistributedActorWrapper<t_CActor0>, NConcurrency::TCDistributedActorWrapper<t_CActor1>, t_CAllocator0, t_CAllocator1>
			{
				enum
				{
					mc_Value = TCIsValidConversion<t_CActor0, t_CActor1, t_CAllocator0, t_CAllocator1>::mc_Value
				};
			};
		}
	}
}
namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			
			template <typename t_CType>
			struct TCRemoveContinuation
			{
				using CType = t_CType;
			};
			
			template <typename t_CType>
			struct TCRemoveContinuation<TCContinuation<t_CType>>
			{
				using CType = t_CType;
			};

			struct CSubSystem_Concurrency_DistributedActor : public CSubSystem
			{
				CSubSystem_Concurrency_DistributedActor();
				~CSubSystem_Concurrency_DistributedActor();
				
				TCActor<CActor> m_TestActor;
				TCActor<CActorDistributionManager> m_DistributionManager;
				
				void f_DestroyThreadSpecific() override;
			};
			
			CSubSystem_Concurrency_DistributedActor &fg_DistributedActorSubSystem();

			template <typename tf_CResult>
			bool fg_CopyReplyToContinuationShared(NStream::CBinaryStreamMemoryPtr<> &_Stream, TCContinuation<tf_CResult> &_Continuation)
			{
				uint8 bException;
				_Stream >> bException;
				
				if (bException)
				{
					uint32 TypeHash;
					_Stream >> TypeHash;
					
					auto &TypeRegistry = fg_RuntimeTypeRegistry();
					auto pEntry = TypeRegistry.m_EntryByHash_Exception.f_FindEqual(TypeHash);
					
					if (!pEntry)
					{
						_Continuation.f_SetException(DMibErrorInstance("Unknown exception type received"));
						return true;
					}
					
					_Continuation.f_SetException(pEntry->f_Consume(_Stream));
					
					return true;
				}
				return false;
			}
			
			template <typename tf_CResult>
			void fg_CopyReplyToContinuation(TCContinuation<tf_CResult> &_Continuation, NContainer::TCVector<uint8> const &_Data)
			{
				NStream::CBinaryStreamMemoryPtr<> ReplyStream;
				ReplyStream.f_OpenRead(_Data);
				if (fg_CopyReplyToContinuationShared(ReplyStream, _Continuation))
					return;
				tf_CResult Result;
				ReplyStream >> Result;
				
				_Continuation.f_SetResult(fg_Move(Result));
			}

			template <>
			void fg_CopyReplyToContinuation(TCContinuation<void> &_Continuation, NContainer::TCVector<uint8> const &_Data);
		}
		
		template <typename t_CActor>
		template <typename... tf_CActor>
		TCDistributedActorWrapper<t_CActor>::TCDistributedActorWrapper(tf_CActor &&...p_Params)
			: t_CActor(fg_Forward<tf_CActor>(p_Params)...)
		{
		}

		template <typename tf_CActor, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(tfp_CParams &&...p_Params)
		{
			return fg_ConstructActor<TCDistributedActorWrapper<tf_CActor>>(fg_Forward<tfp_CParams>(p_Params)...);
		}
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
		auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params)
		{
			using CReturn = typename NPrivate::TCRemoveContinuation<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CReturn>::CType;
			
			NFunction::TCFunction<TCContinuation<CReturn> (NFunction::CThisTag &)> ToDispatch;
			if (1) // Only when remote
			{
				DMibConcurrencyRegisterMemberFunction(tf_CMemberFunction, t_pMemberFunction, t_NameHash);
				NStream::CBinaryStreamMemory<> Stream;
				Stream << t_NameHash;
				std::initializer_list<bool> Dummy = 
					{
						[&]
						{
							Stream << p_Params; return true;
						}()...
					}
				;
				(void)Dummy;
				
				ToDispatch = [_Actor, Data = Stream.f_MoveVector()]() mutable
					{
						TCContinuation<CReturn> Continuation;
						
						auto &TypeRegistry = fg_RuntimeTypeRegistry();
						
						NStream::CBinaryStreamMemoryPtr<> Stream;
						Stream.f_OpenRead(Data);
						
						uint32 FunctionHash;
						Stream >> FunctionHash;
						
						auto pEntry = TypeRegistry.m_EntryByHash_MemberFunction.f_FindEqual(FunctionHash);
						
						if (!pEntry)
						{
							Continuation.f_SetException(DMibErrorInstance("Member function entry not found"));
							return Continuation;
						}

						NConcurrency::TCContinuation<NContainer::TCVector<uint8>> Return = pEntry->f_Call
							(
								Stream
								, &_Actor->f_AccessInternal()
							)
						;
						
						Return.f_OnResultSet
							(
								[Continuation](auto &&_Result) mutable
								{
									if (!_Result)
										Continuation.f_SetException(fg_Move(_Result));
									else
										NPrivate::fg_CopyReplyToContinuation(Continuation, *_Result);
								}
							)
						;
						
						return Continuation;
					}
				;
			}
			else // When local
			{
				ToDispatch = [_Actor, p_Params...]
					{
						TCContinuation<CReturn> Continuation;
						_Actor(t_pMemberFunction, p_Params...)
							> fg_AnyConcurrentActor() / [Continuation](TCAsyncResult<CReturn> &&_Result)
							{
								Continuation.f_SetResult(fg_Move(_Result));
							}
						;
						return Continuation;
					}
				;
			}
			auto &SubSystem = NPrivate::fg_DistributedActorSubSystem();
			
			return SubSystem.m_TestActor.f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<CReturn>>
					, ToDispatch
				)
			;
		}
		
		template <typename tf_CStream>
		void CActorDistributionCryptographySettings::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << m_PrivateCertificate;
			_Stream << m_PublicCertificate;
			_Stream << m_RemoteClientCertificates;
			_Stream << m_SignedClientCertificates;
			_Stream << m_Serial;
			_Stream << m_Subject;
		}
		
		template <typename tf_CStream>
		void CActorDistributionCryptographySettings::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PrivateCertificate;
			_Stream >> m_PublicCertificate;
			_Stream >> m_RemoteClientCertificates;
			_Stream >> m_SignedClientCertificates;
			_Stream >> m_Serial;
			_Stream >> m_Subject;
		}
	
		template <typename tf_CStream>
		void CActorDistributionCryptographyRemoteServer::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << m_PublicServerCertificate;
			_Stream << m_PublicClientCertificate;
		}
		
		template <typename tf_CStream>
		void CActorDistributionCryptographyRemoteServer::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PublicServerCertificate;
			_Stream >> m_PublicClientCertificate;
		}
	}
}
