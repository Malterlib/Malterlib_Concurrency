// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction, typename t_CReturn = typename NTraits::TCMemberFunctionPointerTraits<t_CMemberFunction>::CReturn>
		struct TCCallToContinuation
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCContinuation<t_CReturn> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				TCContinuation<t_CReturn> Return;
				Return.f_SetResult((_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...));
				return Return;
			}
		};

		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction, typename t_CReturn>
		struct TCCallToContinuation<t_CMemberFunction, t_fMemberFunction, TCContinuation<t_CReturn>>
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCContinuation<t_CReturn> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				return (_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...);
			}
		};

		template <typename t_CMemberFunction, t_CMemberFunction t_fMemberFunction>
		struct TCCallToContinuation<t_CMemberFunction, t_fMemberFunction, void>
		{
			template <typename tf_CClass, typename ...tfp_CParam>
			static TCContinuation<void> fs_Call(tf_CClass *_pObject, tfp_CParam && ...p_Params)
			{
				TCContinuation<void> Return;
				(_pObject->*t_fMemberFunction)(fg_Forward<tfp_CParam>(p_Params)...);
				Return.f_SetResult();
				return Return;
			}
		};
	}

	template 
	<
		typename tf_CMemberFunction
		, tf_CMemberFunction t_pMemberFunction
		, uint32 t_NameHash
		, typename tf_CActor
		, typename... tfp_CParams
	>
	auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params)
	{
		using CReturn = typename NPrivate::TCRemoveContinuation<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CReturn>::CType;
		
		NFunction::TCFunctionMovable<TCContinuation<CReturn> ()> ToDispatch;
		
		auto *pActorDataRaw = static_cast<NPrivate::CDistributedActorData *>(_Actor->f_GetDistributedActorData().f_Get());

		TCActor<> DispatchActor;

		do
		{
			if (pActorDataRaw && pActorDataRaw->m_bRemote) // Only when remote
			{
				CDistributedActorWriteStream Stream;
				Stream << uint8(0); // Dummy command
				Stream << uint64(0); // Dummy packet ID
				Stream << pActorDataRaw->m_ActorID;				
				Stream << t_NameHash;
				Stream << pActorDataRaw->m_ProtocolVersion;
				
				auto pHost = pActorDataRaw->m_pHost.f_Lock();
				if (!pHost || pHost->m_bDeleted)
				{
					ToDispatch = []
						{
							TCContinuation<CReturn> Continuation;
							Continuation.f_SetException(DMibErrorInstance("Remote actor host no longer available"));
							return Continuation;
						}
					;
					break;
				}
				
				NPrivate::CDistributedActorStreamContext Context{pHost->m_ActorProtocolVersion.f_Load(), true};
				
				NPrivate::TCStreamArguments<typename NTraits::TCMemberFunctionPointerTraits<tf_CMemberFunction>::CParams>::fs_Stream
					(
						Stream
						, Context
						, pActorDataRaw->m_ProtocolVersion
						, fg_Forward<tfp_CParams>(p_Params)...
					)
				;
				
				auto pActorData = NPtr::TCSharedPointer<NPrivate::CDistributedActorData>{pActorDataRaw};

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
						 , Version = pActorDataRaw->m_ProtocolVersion
					]
					() mutable
					{
						TCContinuation<CReturn> Continuation;

						if (!DistributionManager)
						{
							Continuation.f_SetException(DMibErrorInstance("Actor distribution manager for actor no longer exists"));
							return Continuation;
						}
						auto *pDistributionManager = NPrivate::fg_GetInternalActor(DistributionManager);
						pDistributionManager->f_CallRemote(fg_Move(pActorData), fg_Move(Data), Context)
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
				DispatchActor = _Actor;
				ToDispatch = [_Actor, Params = NContainer::fg_Tuple(fg_Forward<tfp_CParams>(p_Params)...)]() mutable
					{
						return NContainer::fg_TupleApplyAs<NMeta::TCTypeList<typename NTraits::TCDecayForward<tfp_CParams>::CType...>>
							(
								[&](auto &&..._Params) mutable -> TCContinuation<CReturn>
								{
									auto *pActor = NPrivate::fg_GetInternalActor(_Actor);
									return NPrivate::TCCallToContinuation<tf_CMemberFunction, t_pMemberFunction>::fs_Call(pActor, fg_Forward<decltype(_Params)>(_Params)...);
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
		return DispatchActor.f_CallByValue
			(
				&CActor::f_DispatchWithReturn<TCContinuation<CReturn>>
				, fg_Move(ToDispatch)
			)
		;
	}
}
