// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "Malterlib_Concurrency_Manager.h"

#include <Mib/Cryptography/RandomID>

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
		template <typename tf_CActor, typename tf_CActorSource>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_StaticCast(TCActor<TCDistributedActorWrapper<tf_CActorSource>> const &_Actor)
		{
			auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
			(void)pDummy;

			return reinterpret_cast<TCActor<TCDistributedActorWrapper<tf_CActor>> const &>(_Actor);
		}
		
		
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
				
				TCActor<CActorDistributionManager> m_DistributionManager;
				
				struct CThreadLocal
				{
					NStr::CStr m_CallingHostID;
				};
				
				NThread::TCThreadLocal<CThreadLocal> m_ThreadLocal;
				
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
			void fg_CopyReplyToContinuation(TCContinuation<tf_CResult> &_Continuation, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data)
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
			void fg_CopyReplyToContinuation(TCContinuation<void> &_Continuation, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure> const &_Data);
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
			auto &ConcurrencyManager = fg_ConcurrencyManager();

			NPtr::TCSharedPointer<NPrivate::CDistributedActorData> pDistributedActorData = fg_Construct();
			pDistributedActorData->m_ActorID = NCryptography::fg_RandomID();
			
			NPtr::TCSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActor>>, NPtr::CSupportWeakTag, CInternalActorAllocator> pActor 
				= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData))
			;
			
			return ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<tf_CActor>>
				(
					fg_Move(pActor)
					, fg_Construct<TCDistributedActorWrapper<tf_CActor>>(fg_Forward<tfp_CParams>(p_Params)...)
				)
			;
		}
		
		namespace NPrivate
		{
			template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams, mint... tfp_Indidies>
			TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActorHelper(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, NMeta::TCIndices<tfp_Indidies...> const&, tfp_CParams &&...p_Params)
			{
				auto &ConcurrencyManager = fg_ConcurrencyManager();
				
				NPtr::TCSharedPointer<NPrivate::CDistributedActorData> pDistributedActorData = fg_Construct();
				pDistributedActorData->m_ActorID = NCryptography::fg_RandomID();
				
				NPtr::TCSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActor>>, NPtr::CSupportWeakTag, CInternalActorAllocator> pActor 
					= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData))
				;
				
				return ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<tf_CActor>>
					(
						fg_Move(pActor)
						, fg_Construct<TCDistributedActorWrapper<tf_CActor>>(fg_Forward<tfp_CParams>(p_Params)...)
						, fg_Forward<tfp_CHolderParams>(NContainer::fg_Get<tfp_Indidies>(_HolderParams.m_Params))...
					)
				;
			}
		}
		
		template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams, tfp_CParams &&...p_Params)
		{
			return NPrivate::fg_ConstructActorHelper<tf_CActor>
				(
					fg_Move(_HolderParams)
					, typename NMeta::TCMakeConsecutiveIndices<sizeof...(tfp_CHolderParams)>::CType()
					, fg_Forward<tfp_CParams>(p_Params)...
				)
			;
		}
		
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
		auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params)
		{
			using CReturn = typename NPrivate::TCRemoveContinuation<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CReturn>::CType;
			
			NFunction::TCFunction<TCContinuation<CReturn> (NFunction::CThisTag &)> ToDispatch;
			
			auto *pActorDataRaw = static_cast<NPrivate::CDistributedActorData *>(_Actor->f_GetDistributedActorData().f_Get());
			
			if (pActorDataRaw && pActorDataRaw->m_bRemote) // Only when remote
			{
				DMibConcurrencyRegisterMemberFunction(tf_CMemberFunction, t_pMemberFunction, t_NameHash);
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
				Stream << uint8(0); // Dummy command
				Stream << uint64(0); // Dummy packet ID
				Stream << pActorDataRaw->m_ActorID;				
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
				
				auto pActorData = NPtr::TCSharedPointer<NPrivate::CDistributedActorData>{pActorDataRaw};
				
				ToDispatch = [_Actor, Data = Stream.f_MoveVector(), pActorData = fg_Move(pActorData)]() mutable
					{
						TCContinuation<CReturn> Continuation;
						
						TCActor<CActorDistributionManager> const &DistributionManager = fg_GetDistributionManager();
						
						DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), fg_Move(Data))
							> [Continuation](TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result) mutable
							{
								if (!_Result)
								{
									Continuation.f_SetException(fg_Move(_Result));
									return;
								}
								try
								{
									NPrivate::fg_CopyReplyToContinuation(Continuation, *_Result);
								}
								catch (NException::CException const &_Exception)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("Exception reading remote result: {}", _Exception.f_GetErrorStr())));
								}								
							}
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
			return fg_AnyConcurrentActor().f_CallByValue
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
