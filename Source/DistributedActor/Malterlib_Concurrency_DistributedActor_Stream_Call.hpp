// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> &&_Actor, tfp_CParams && ...p_Params)
	{
		constexpr static uint32 c_NameHash = ::NMib::fg_GetMemberFunctionHash<tf_pMemberFunction>(DMibIfNotSupportMemberNameFromMemberPointer(tf_NameHash));
		using CMemberFunction = decltype(tf_pMemberFunction);
		using COriginalReturn = typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CReturn;
		using CReturn = typename NPrivate::TCRemoveFuture<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CReturn>::CType;

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
					ToDispatch = []
						{
							return TCPromise<CReturn>() <<= DMibErrorInstance("The remote is using an older protocol version not supported for this function");
						}
					;
					DispatchActor = fg_DirectCallActor();
					break;
				}
				CDistributedActorWriteStream Stream;
				Stream << uint8(0); // Dummy command
				Stream << uint64(0); // Dummy packet ID
				Stream << pActorDataRaw->m_ActorID;
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

				auto pHost = pActorDataRaw->m_pHost.f_Lock();
				if (!pHost || pHost->m_bDeleted)
				{
					ToDispatch = []
						{
							return TCPromise<CReturn>() <<= DMibErrorInstance("Remote actor host no longer available");
						}
					;
					DispatchActor = fg_DirectCallActor();
					break;
				}

				NPrivate::CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion.f_Load(), true};

				NPrivate::TCStreamArguments<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CParams>::fs_Stream
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
					ToDispatch = []
						{
							return TCPromise<CReturn>() <<= DMibErrorInstance("Actor distribution manager for actor no longer exists");
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
					() mutable
					{
						TCPromise<CReturn> Promise;
						pDistributionManager->f_CallRemote(fg_Move(pActorData), fg_Move(Data), Context)
							> [Promise, Context, ProtocolVersion](TCAsyncResult<NContainer::CSecureByteVector> &&_Result) mutable
							{
								if (!_Result)
								{
									Promise.f_SetException(fg_Move(_Result));
									return;
								}
								try
								{
									NException::CDisableExceptionTraceScope DisableTrace;
									NPrivate::fg_CopyReplyToPromise(Promise, *_Result, Context, ProtocolVersion);
								}
								catch (NException::CException const &_Exception)
								{
									Promise.f_SetException(DMibErrorInstance(fg_Format("Exception reading remote result: {}", _Exception.f_GetErrorStr())));
								}
							}
						;
						return Promise.f_MoveFuture();
					}
				;
			}
			else // When local
			{
				using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CParams>::CMoveList;
				using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CParams>::CType;

				auto pActor = NPrivate::fg_GetInternalActor(_Actor);
				DispatchActor = fg_Move(_Actor);

				if constexpr (NPrivate::TCIsAsyncGenerator<COriginalReturn>::mc_Value)
				{
					ToDispatch = [pActor, Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...)]() mutable mark_no_coroutine_debug -> TCFuture<CReturn>
						{
							return NStorage::fg_TupleApplyAs<CMoveList>
								(
									[&](auto &&..._Params) mutable -> TCFuture<CReturn>
									{
										return TCPromise<CReturn>() <<= fg_CallSafe
											(
												static_cast<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CClass *>(pActor)
												, tf_pMemberFunction
												, fg_Forward<decltype(_Params)>(_Params)...
											)
										;
									}
									, fg_Move(Params)
								)
							;
						}
					;
				}
				else if constexpr (NPrivate::TCIsFuture<COriginalReturn>::mc_Value)
				{
					ToDispatch = [pActor, Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...)]() mutable mark_no_coroutine_debug -> TCFuture<CReturn>
						{
							return NStorage::fg_TupleApplyAs<CMoveList>
								(
									[&](auto &&..._Params) mutable mark_no_coroutine_debug -> TCFuture<CReturn>
									{
										return (pActor->*tf_pMemberFunction)(fg_Forward<decltype(_Params)>(_Params)...);
									}
									, fg_Move(Params)
								)
							;
						}
					;
				}
				else if constexpr (NTraits::TCIsVoid<COriginalReturn>::mc_Value)
				{
					ToDispatch = [pActor, Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...)]() mutable
						{
							return NStorage::fg_TupleApplyAs<CMoveList>
								(
									[&](auto &&..._Params) mutable -> TCFuture<CReturn>
									{
										TCPromise<void> Promise;
										(pActor->*tf_pMemberFunction)(fg_Forward<decltype(_Params)>(_Params)...);
										return Promise <<= g_Void;
									}
									, fg_Move(Params)
								)
							;
						}
					;
				}
				else
				{
					ToDispatch = [pActor, Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...)]() mutable
						{
							return NStorage::fg_TupleApplyAs<CMoveList>
								(
									[&](auto &&..._Params) mutable -> TCFuture<CReturn>
									{
										return TCPromise<CReturn>() <<= (pActor->*tf_pMemberFunction)(fg_Forward<decltype(_Params)>(_Params)...);
									}
									, fg_Move(Params)
								)
							;
						}
					;
				}
			}
		}
		while (false)
			;
		_Actor.f_Clear();
		return fg_Move(DispatchActor).f_CallByValue<&CActor::f_DispatchWithReturn<TCFuture<CReturn>>>(fg_Move(ToDispatch));
	}
}
