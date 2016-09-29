// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "../Actor/Malterlib_Concurrency_Manager.h"

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
		template <typename tf_CValue>
		void fg_DistributedActorParamsFeed(CDistributedActorWriteStream &_Stream, tf_CValue const &_Value)
		{
			NPrivate::CDistributedActorStreamContextSending *pContext = (NPrivate::CDistributedActorStreamContextSending *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());
			decltype(auto) ToStream = pContext->f_GetValueForFeed(_Value);
			_Stream << ToStream;
		}
		
		template <typename tf_CValue>
		void fg_DistributedActorParamsConsume(CDistributedActorReadStream &_Stream, tf_CValue &_Value)
		{
			NPrivate::CDistributedActorStreamContextReceiving *pContext = (NPrivate::CDistributedActorStreamContextReceiving *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());
			decltype(auto) ToStream = pContext->f_GetValueForConsume(_Value); 
			_Stream >> ToStream;
		}
		
		template <typename tf_CValue>
		void fg_DistributedActorReturnFeed(CDistributedActorWriteStream &_Stream, tf_CValue const &_Value)
		{
			NPrivate::CDistributedActorStreamContextReceiving *pContext = (NPrivate::CDistributedActorStreamContextReceiving *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());
			decltype(auto) ToStream = pContext->f_GetValueForFeed(_Value); 
			_Stream << ToStream;
		}
		
		template <typename tf_CValue>
		void fg_DistributedActorReturnConsume(CDistributedActorReadStream &_Stream, tf_CValue &_Value)
		{
			NPrivate::CDistributedActorStreamContextSending *pContext = (NPrivate::CDistributedActorStreamContextSending *)_Stream.f_GetContext();
			DMibFastCheck(pContext && pContext->f_CorrectMagic());
			decltype(auto) ToStream = pContext->f_GetValueForConsume(_Value); 
			_Stream >> ToStream;
		}
		
		template <typename ...tfp_CToPublish>
		CDistributedActorInheritanceHeirarchyPublish CDistributedActorInheritanceHeirarchyPublish::fs_GetHierarchy()
		{
			CDistributedActorInheritanceHeirarchyPublish Ret;
			
			CDistributedActorProtocolVersions Versions;
			
			TCInitializerList<bool> Dummy = 
				{
					[&]
					{
						CDistributedActorProtocolVersions ThisVersions{tfp_CToPublish::EMinProtocolVersion, tfp_CToPublish::EProtocolVersion};
						if (Versions.m_MinSupported == TCLimitsInt<uint32>::mc_Max)
							Versions = ThisVersions;
						else 
							DMibFastCheck(ThisVersions == Versions); // Can only publish one protocol version
						Ret.mp_Hierarchy.f_Insert(fg_GetTypeHash<tfp_CToPublish>());
						return true;
					}
					()...
				}
			;
			(void)Dummy;

			Ret.mp_ProtocolVersions = Versions;
			
			return Ret;
		}
		NContainer::TCVector<uint32> const &CDistributedActorInheritanceHeirarchyPublish::f_GetHierarchy() const
		{
			return mp_Hierarchy;
		}
		
		CDistributedActorProtocolVersions const & CDistributedActorInheritanceHeirarchyPublish::f_GetProtocolVersions() const
		{
			return mp_ProtocolVersions;
		}
		
		template <typename tf_CActor, typename tf_CActorSource>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_StaticCast(TCActor<TCDistributedActorWrapper<tf_CActorSource>> const &_Actor)
		{
			auto pDummy = static_cast<tf_CActor *>((tf_CActorSource *)nullptr);
			(void)pDummy;

			return reinterpret_cast<TCActor<TCDistributedActorWrapper<tf_CActor>> const &>(_Actor);
		}

		template <typename tf_CFirst>
		NContainer::TCVector<uint32> fg_GetDistributedActorInheritanceHierarchy()
		{
			NContainer::TCVector<uint32> Return;
			
			NPrivate::fg_GetDistributedActorInheritanceHierarchy<tf_CFirst>(Return);
			
			return Return;
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
			
			NFunction::TCFunctionMovable<TCContinuation<CReturn> ()> ToDispatch;
			
			auto *pActorDataRaw = static_cast<NPrivate::CDistributedActorData *>(_Actor->f_GetDistributedActorData().f_Get());
			
			if (pActorDataRaw && pActorDataRaw->m_bRemote) // Only when remote
			{
				DMibConcurrencyRegisterMemberFunctionWithStreamContext(tf_CMemberFunction, t_pMemberFunction, t_NameHash, NPrivate::CDistributedActorStreamContextReceiving);
				NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
				Stream << uint8(0); // Dummy command
				Stream << uint64(0); // Dummy packet ID
				Stream << pActorDataRaw->m_ActorID;				
				Stream << t_NameHash;
				Stream << pActorDataRaw->m_ProtocolVersion;
				
				NPrivate::CDistributedActorStreamContextSending Context;
				
				NPrivate::TCStreamArguments<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CParams>::fs_Stream
					(
						Stream
						, Context
						, pActorDataRaw->m_ProtocolVersion
						, p_Params...
					)
				;
				
				auto pActorData = NPtr::TCSharedPointer<NPrivate::CDistributedActorData>{pActorDataRaw};
				
				ToDispatch = [_Actor, Data = Stream.f_MoveVector(), pActorData = fg_Move(pActorData), Context, Version = pActorDataRaw->m_ProtocolVersion]() mutable
					{
						TCContinuation<CReturn> Continuation;
						
						TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();
						
						if (!DistributionManager)
						{
							Continuation.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
							return Continuation;
						}

						DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), fg_Move(Data), Context)
							> [Continuation, Context, Version](TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result) mutable
							{
								if (!_Result)
								{
									Continuation.f_SetException(fg_Move(_Result));
									return;
								}
								try
								{
									NException::CDisableExceptionTraceScope DisableTrace;
									NPrivate::fg_CopyReplyToContinuation(Continuation, *_Result, Context, Version);
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
				ToDispatch = [_Actor, Params = NContainer::fg_Tuple(fg_Forward<tfp_CParams>(p_Params)...)]() mutable
					{
						TCContinuation<CReturn> Continuation;
						std::apply
							(
								[&](auto ..._Params) mutable
								{
									_Actor(t_pMemberFunction, fg_Forward<typename NTraits::TCDecayForward<tfp_CParams>::CType>(_Params)...)
										> fg_AnyConcurrentActor() / [Continuation](TCAsyncResult<CReturn> &&_Result)
										{
											Continuation.f_SetResult(fg_Move(_Result));
										}
									;
								}
								, fg_Move(Params) 
							)
						;
						return Continuation;
					}
				;
			}
			return fg_AnyConcurrentActor().f_CallByValue
				(
					&CActor::f_DispatchWithReturn<TCContinuation<CReturn>>
					, fg_Move(ToDispatch)
				)
			;
		}
		
		template <typename t_FFunction, typename t_CReturn, typename ...tp_CParams>
		void NPrivate::TCStreamFunctionReceive<t_FFunction, TCContinuation<t_CReturn> (tp_CParams...)>::f_Consume(CDistributedActorReadStream &_Stream)
		{
			NStr::CStr FunctionID;
			_Stream >> FunctionID;
			m_fFunction = [FunctionID](tp_CParams ...p_Params) -> TCContinuation<t_CReturn>
				{
					auto &ThreadLocal = *fg_DistributedActorSubSystem().m_ThreadLocal;

					auto *pActorDataRaw = static_cast<CDistributedActorData *>(ThreadLocal.m_pCurrentRemoteDispatchActorData);

					if (!pActorDataRaw)
						return DMibErrorInstance("This functions needs to be dispatched on a remote actor");
					
					if (!pActorDataRaw->m_bRemote)
						return DMibErrorInstance("Internal error (missing or incorrect distributed actor data)");
					
					NStream::CBinaryStreamMemory<NStream::CBinaryStreamDefault, NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> Stream;
					Stream << uint8(0); // Dummy command
					Stream << uint64(0); // Dummy packet ID
					Stream << pActorDataRaw->m_ActorID;				
					Stream << 0;
					Stream << pActorDataRaw->m_ProtocolVersion;
					Stream << FunctionID;
					
					CDistributedActorStreamContextSending Context;
					
					TCStreamArguments<NMeta::TCTypeList<tp_CParams...>>::fs_Stream(Stream, Context, pActorDataRaw->m_ProtocolVersion, p_Params...);
					
					auto pActorData = NPtr::TCSharedPointer<CDistributedActorData>{pActorDataRaw};
					
					TCContinuation<t_CReturn> Continuation;
					
					TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();
					
					if (!DistributionManager)
					{
						Continuation.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
						return Continuation;
					}

					DistributionManager(&CActorDistributionManager::f_CallRemote, fg_Move(pActorData), Stream.f_MoveVector(), Context)
						> [Continuation, Context, Version = pActorDataRaw->m_ProtocolVersion](TCAsyncResult<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> &&_Result) mutable
						{
							if (!_Result)
							{
								Continuation.f_SetException(fg_Move(_Result));
								return;
							}
							try
							{
								NException::CDisableExceptionTraceScope DisableTrace;
								fg_CopyReplyToContinuation(Continuation, *_Result, Context, Version);
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
		
		
		template <typename tf_CStream>
		void CActorDistributionCryptographySettings::f_Feed(tf_CStream &_Stream) const
		{
			_Stream << m_PrivateKey;
			_Stream << m_PublicCertificate;
			_Stream << m_RemoteClientCertificates;
			_Stream << m_SignedClientCertificates;
			_Stream << m_Serial;
			_Stream << m_Subject;
		}
		
		template <typename tf_CStream>
		void CActorDistributionCryptographySettings::f_Consume(tf_CStream &_Stream)
		{
			_Stream >> m_PrivateKey;
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
		
		template <typename tf_CType>
		TCDistributedActor<tf_CType> CAbstractDistributedActor::f_GetActor() const
		{
			auto Actor = f_GetActor(fg_GetTypeHash<tf_CType>(), CDistributedActorProtocolVersions{tf_CType::EMinProtocolVersion, tf_CType::EProtocolVersion});
			return fg_Move(reinterpret_cast<TCDistributedActor<tf_CType> &>(Actor));
		}

		inline NStr::CStr const &CAbstractDistributedActor::f_GetUniqueHostID() const
		{
			return mp_UniqueHostID;
		}
		
		inline NStr::CStr const &CAbstractDistributedActor::f_GetRealHostID() const
		{
			return mp_HostInfo.m_HostID;
		}
		
		inline CHostInfo const &CAbstractDistributedActor::f_GetHostInfo() const
		{
			return mp_HostInfo;
		}
		
		inline bool CAbstractDistributedActor::f_IsEmpty() const
		{
			return mp_ActorID.f_IsEmpty();
		}
		
		CDistributedActorProtocolVersions const &CAbstractDistributedActor::f_GetProtocolVersions() const
		{
			return mp_ProtocolVersions;
		}
		
		template <typename tf_CString>
		void CHostInfo::f_Format(tf_CString &o_String) const
		{
			o_String += f_GetDesc();
		}
		
		template <typename tf_CType>
		CDistributedActorProtocolVersions fg_SubscribeVersions()
		{
			return CDistributedActorProtocolVersions{tf_CType::EMinProtocolVersion, tf_CType::EProtocolVersion};
		}
		
		template <typename t_CActor>
		bool operator == (CDistributedActorIdentifier const &_Left, TCActor<t_CActor> const &_Right)
		{
			return _Left == (TCActor<> const &)_Right;
		}
		
		template <typename t_CActor>
		bool operator == (TCActor<t_CActor> const &_Left, CDistributedActorIdentifier const &_Right)
		{
			return (TCActor<> const &)_Left == _Right;
		}
	}
}
