// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, EVirtualCall tf_VirtualCall
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> &&_Actor, tfp_CParams && ...p_Params)
		-> NPrivate::TCFutureReturn<decltype(tf_pMemberFunction)>
		requires cActorCallableWithFunctor<tf_pMemberFunction, tf_CActor, tfp_CParams...>
	{
		constexpr static uint32 c_NameHash = ::NMib::fg_GetMemberFunctionHash<tf_pMemberFunction>(DMibIfNotSupportMemberNameFromMemberPointer(tf_NameHash));
		using CMemberFunction = decltype(tf_pMemberFunction);
		using CFunctionTraits = NTraits::TCMemberFunctionPointerTraits<CMemberFunction>;
		using COriginalReturn = typename CFunctionTraits::CReturn;
		using CReturn = typename NPrivate::TCRemoveFuture<COriginalReturn>::CType;

		NFunction::TCFunctionMovable<TCFuture<CReturn> ()> ToDispatch;

		auto *pActorDataRaw = static_cast<NPrivate::CDistributedActorData *>(_Actor->f_GetDistributedActorData().f_Get());

		TCActor<> DispatchActor;

		do
		{
			if (pActorDataRaw && pActorDataRaw->m_bRemote) // Only when remote
			{
				auto ProtocolVersion = pActorDataRaw->m_ProtocolVersion;

				if (ProtocolVersion < TCLowestSupportedVersionForMemberFunction<tf_pMemberFunction>::mc_Value)
				{
					ToDispatch = []() -> TCFuture<CReturn>
						{
							co_return DMibErrorInstance("The remote is using an older protocol version not supported for this function");
						}
					;
					DispatchActor = fg_DirectCallActor();
					break;
				}
				auto pHost = pActorDataRaw->m_pHost.f_Lock();
				if (!pHost || pHost->m_bDeleted.f_Load(NAtomic::EMemoryOrder_Relaxed))
				{
					ToDispatch = []() -> TCFuture<CReturn>
						{
							co_return DMibErrorInstance("Remote actor host no longer available");
						}
					;
					DispatchActor = fg_DirectCallActor();
					break;
				}

				uint32 HostActorProtocolVersion = pHost->m_ActorProtocolVersion.f_Load(NAtomic::EMemoryOrder_Relaxed);
				uint32 AuthHandlerID = CAuthenticationHandlerIDScope::fs_GetCurrentHandlerID();
				bool bUseAuthHandler = AuthHandlerID != 0 && HostActorProtocolVersion >= EDistributedActorProtocolVersion_MultipleAuthenticationHandlers;
				uint8 Priority = CPriorityScope::fs_GetCurrentPriority();
				bool bUsePriority = Priority != 128 && HostActorProtocolVersion >= EDistributedActorProtocolVersion_PrioritySupport;

				CDistributedActorWriteStream Stream;
				if (bUsePriority)
				{
					if (bUseAuthHandler)
						Stream << uint8(16); // EDistributedActorCommand_RemoteCallWithPriorityAndAuthHandler
					else
						Stream << uint8(15); // EDistributedActorCommand_RemoteCallWithPriority
					Stream << Priority;
				}
				else
				{
					if (bUseAuthHandler)
						Stream << uint8(14); // EDistributedActorCommand_RemoteCallWithAuthHandler
					else
						Stream << uint8(2); // EDistributedActorCommand_RemoteCall
				}
				Stream << uint64(0); // Dummy packet ID
				Stream << pActorDataRaw->m_ActorID;
				if (bUseAuthHandler)
					Stream << AuthHandlerID;
				uint32 NameHash = c_NameHash;
				if constexpr (TCAlternateHashesForMemberFunction<tf_pMemberFunction>::mc_bHasAlternates)
				{
					for (auto &AlternateHash : TCAlternateHashesForMemberFunction<tf_pMemberFunction>::CAlternatesArray::mc_Value)
					{
						if (ProtocolVersion <= AlternateHash.m_UpToVersion)
							NameHash = AlternateHash.m_Hash;
					}
				}

				Stream << NameHash;
				Stream << ProtocolVersion;

				NPrivate::CDistributedActorStreamContext Context{HostActorProtocolVersion, true};

				NPrivate::TCStreamArguments<typename CFunctionTraits::CParams>::fs_Stream
					(
						Stream
						, Context
						, ProtocolVersion
						, fg_Forward<tfp_CParams>(p_Params)...
					)
				;

				auto pActorData = NStorage::TCSharedPointer<NPrivate::CDistributedActorData>{pActorDataRaw};

				TCActor<CActorDistributionManager> DistributionManager = pActorData->m_DistributionManager.f_Lock();

				if (!DistributionManager)
				{
					ToDispatch = []() -> TCFuture<CReturn>
						{
							co_return DMibErrorInstance("Actor distribution manager for actor no longer exists");
						}
					;
					DispatchActor = fg_DirectCallActor();
					break;
				}

				auto pDistributionManager = NPrivate::fg_GetInternalActor(DistributionManager);
				DispatchActor = fg_Move(DistributionManager);

				ToDispatch =
					[
						pDistributionManager
						, Data = Stream.f_MoveVector()
						, pActorData = fg_Move(pActorData)
						, Context
						, ProtocolVersion
					]
					() mutable -> TCFuture<CReturn>
					{
						auto Result = co_await pDistributionManager->f_CallRemote(fg_Move(pActorData), fg_Move(Data), Context).f_Wrap();

						if (!Result)
							co_return fg_Move(Result).f_GetException();

						try
						{
							NException::CDisableExceptionTraceScope DisableTrace;
							TCAsyncResult<CReturn> AsyncResult;
							NPrivate::fg_CopyReplyToAsyncResult(AsyncResult, *Result, Context, ProtocolVersion);

							co_return fg_Move(AsyncResult);
						}
						catch (NException::CException const &)
						{
							co_return DMibErrorInstance(fg_Format("Exception reading remote result: {}", NException::fg_CurrentExceptionString()));
						}
					}
				;
			}
			else // When local
			{
				auto pActor = NPrivate::fg_GetInternalActor(_Actor);
				DispatchActor = fg_Move(_Actor);

				if constexpr (NPrivate::TCIsFuture<COriginalReturn>::mc_Value)
				{
					[&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&)
					{
						ToDispatch =
							[
								pActor
								, ...p_Params2 = NTraits::TCDecay<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))
							] mark_no_coroutine_debug
							() mutable -> TCFuture<CReturn>
							{
								return [&]<typename ...tfp_CParams3> mark_no_coroutine_debug (tfp_CParams3 &&...p_Params) mutable -> TCFuture<CReturn>
									{
										return (pActor->*tf_pMemberFunction)(fg_Move(p_Params)...);
									}
									(fg_Move(p_Params2)...)
								;
							}
						;
					}
					(typename CFunctionTraits::CParams());
				}
				else if constexpr (NTraits::cIsVoid<COriginalReturn>)
				{
					[&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&)
					{
						ToDispatch = [pActor, ...p_Params2 = NTraits::TCDecay<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))]() mutable -> TCFuture<CReturn>
							{
								return []<typename tf_CActor2, typename ...tfp_CParams3>(tf_CActor2 *_pActor, tfp_CParams3 ...p_Params) mutable -> TCFuture<CReturn>
									{
										(_pActor->*tf_pMemberFunction)(fg_Move(p_Params)...);
										co_return {};
									}
									(pActor, fg_Move(p_Params2)...)
								;
							}
						;
					}
					(typename CFunctionTraits::CParams());
				}
				else
				{
					[&]<typename ...tfp_CParams2>(NMeta::TCTypeList<tfp_CParams2...> &&)
					{
						ToDispatch = [pActor, ...p_Params2 = NTraits::TCDecay<tfp_CParams2>(fg_Forward<tfp_CParams>(p_Params))]() mutable -> TCFuture<CReturn>
							{
								return []<typename tf_CActor2, typename ...tfp_CParams3>(tf_CActor2 *_pActor, tfp_CParams3 ...p_Params) mutable -> TCFuture<CReturn>
									{
										co_return (_pActor->*tf_pMemberFunction)(fg_Move(p_Params)...);
									}
									(pActor, fg_Move(p_Params2)...)
								;
							}
						;
					}
					(typename CFunctionTraits::CParams());
				}
			}
		}
		while (false)
			;
		_Actor.f_Clear();
		return fg_Move(DispatchActor).f_Bind<&CActor::f_DispatchWithReturn<TCFuture<CReturn>>, EVirtualCall::mc_NotVirtual>(fg_Move(ToDispatch)).f_Call();
	}
}
