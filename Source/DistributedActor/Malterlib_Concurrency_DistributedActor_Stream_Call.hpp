// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CReturn>
		struct TCCallToFuture
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCFuture<t_CReturn> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				return fg_Explicit((_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...));
			}
		};

		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction, typename t_CReturn>
		struct TCCallToFuture<t_CMemberFunction, t_fMemberFunction, TCFuture<t_CReturn>>
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCFuture<t_CReturn> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				return (_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...);
			}
		};

		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction>
		struct TCCallToFuture<t_CMemberFunction, t_fMemberFunction, void>
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCFuture<void> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				(_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...);
				return fg_Explicit();
			}
		};
	}

	template 
	<
		auto tf_pMemberFunction
		DMibIfNotSupportMemberNameFromMemberPointer(, uint32 tf_NameHash)
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params)
	{
		constexpr static uint32 c_NameHash = ::NMib::fg_GetMemberFunctionHash<tf_pMemberFunction>(DMibIfNotSupportMemberNameFromMemberPointer(tf_NameHash));
		using CMemberFunction = decltype(tf_pMemberFunction);
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
							TCPromise<CReturn> Promise;
							Promise.f_SetException(DMibErrorInstance("The remote is using an older protocol version not supported for this function"));
							return Promise.f_MoveFuture();
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
							TCPromise<CReturn> Promise;
							Promise.f_SetException(DMibErrorInstance("Remote actor host no longer available"));
							return Promise.f_MoveFuture();
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
					DispatchActor = _Actor;
				else
					DispatchActor = DistributionManager;

				ToDispatch =
					[
						 DistributionManager = fg_Move(DistributionManager)
						 , Data = Stream.f_MoveVector()
						 , pActorData = fg_Move(pActorData)
						 , Context
						 , ProtocolVersion
					]
					() mutable
					{
						TCPromise<CReturn> Promise;

						if (!DistributionManager)
						{
							Promise.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
							return Promise.f_MoveFuture();
						}
						auto *pDistributionManager = NPrivate::fg_GetInternalActor(DistributionManager);
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
				DispatchActor = _Actor;
				using CMoveList = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CParams>::CMoveList;
				using CTupleType = typename NPrivate::TCDecayedTupleHelper<typename NTraits::TCMemberFunctionPointerTraits<CMemberFunction>::CParams>::CType;
				ToDispatch = [_Actor, Params = CTupleType(fg_Forward<tfp_CParams>(p_Params)...)]() mutable
					{
						return NStorage::fg_TupleApplyAs<CMoveList>
							(
								[&](auto &&..._Params) mutable -> TCFuture<CReturn>
								{
									auto *pActor = NPrivate::fg_GetInternalActor(_Actor);
									return NPrivate::TCCallToFuture<CMemberFunction, tf_pMemberFunction>::fs_Call(pActor, fg_Forward<decltype(_Params)>(_Params)...);
								}
								, fg_Move(Params) 
							)
						;
					}
				;
			}
		}
		while (false)
			;
		return DispatchActor.f_CallByValue<&CActor::f_DispatchWithReturn<TCFuture<CReturn>>>(fg_Move(ToDispatch));
	}
}
