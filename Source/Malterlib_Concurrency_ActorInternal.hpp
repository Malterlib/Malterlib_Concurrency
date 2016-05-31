// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		
		template <typename t_CActor>
		inline_always CConcurrencyManager &TCActorInternal<t_CActor>::f_ConcurrencyManager() const
		{
			return *this->mp_pConcurrencyManager;
		}
		
		template <typename t_CActor>
		inline_always EPriority TCActorInternal<t_CActor>::f_GetPriority() const
		{
			return this->mp_Priority;
		}
		
		template <>
		inline_always CConcurrencyManager &TCActorInternal<CAnyConcurrentActor>::f_ConcurrencyManager() const
		{
			return fg_ConcurrencyManager();
		}
		
		template <>
		inline_always EPriority TCActorInternal<CAnyConcurrentActor>::f_GetPriority() const
		{
			return EPriority_Normal;
		}

		template <>
		inline_always CConcurrencyManager &TCActorInternal<CAnyConcurrentActorLowPrio>::f_ConcurrencyManager() const
		{
			return fg_ConcurrencyManager();
		}
		
		template <>
		inline_always EPriority TCActorInternal<CAnyConcurrentActorLowPrio>::f_GetPriority() const
		{
			return EPriority_Low;
		}
		
		template <typename t_CActor>
		t_CActor *TCActorInternal<t_CActor>::fp_GetActor() const
		{
			return static_cast<t_CActor *>(this->mp_pActor.f_Get());
		}

		template <typename t_CActor>
		TCActorInternal<t_CActor>::TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData)
			: CSuper(_pConcurrencyManager, t_CActor::mc_bImmediateDelete, (EPriority)t_CActor::mc_Priority, fg_Move(_pDistributedActorData))
		{
		}

		template <typename t_CActor>
		template <typename tf_CP0>
		TCActorInternal<t_CActor>::TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData, tf_CP0 &&_P0)
			: CSuper(_pConcurrencyManager, t_CActor::mc_bImmediateDelete, (EPriority)t_CActor::mc_Priority, fg_Move(_pDistributedActorData), fg_Forward<tf_CP0>(_P0))
		{
		}

		template <typename t_CActor>
		TCActorInternal<t_CActor>::~TCActorInternal()
		{
			this->f_DestroyThreaded();
		}


		template <typename t_CActor>
		t_CActor &TCActorInternal<t_CActor>::f_AccessInternal()
		{
			static_assert(t_CActor::mc_bAllowInternalAccess, "You do not have internal access to this actor");

			return *fp_GetActor();
		}

		template <typename t_CActor>
		t_CActor const &TCActorInternal<t_CActor>::f_AccessInternal() const
		{
			static_assert(t_CActor::mc_bAllowInternalAccess, "You do not have internal access to this actor");

			return *fp_GetActor();
		}

		template <typename tf_CToDelete>
		auto fg_DeleteWeakObject(CInternalActorAllocator& _Allocator, tf_CToDelete *_pObject)
			-> typename TCEnableIf<NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value, void>::CType
		{
			if (_pObject->f_ImmediateDelete())
			{
				_pObject->~tf_CToDelete();
				if (_pObject->f_WeakRefCountDecrease() == 0)
					_Allocator.f_Free(_pObject);
			}
			else
			{
				auto *pAllocator = &_Allocator;
				
				_pObject->f_ConcurrencyManager().f_DispatchFirstOnCurrentThread
					(
						_pObject->f_GetPriority()
						, [_pObject, pAllocator]
						{
							_pObject->~tf_CToDelete();
							if (_pObject->f_WeakRefCountDecrease() == 0)
								pAllocator->f_Free(_pObject);
						}
					)
				;
			}
		}
		
		template <typename t_CActor>
		template 
			<
				typename tf_CFunction
				, typename tf_CFunctor
				, typename tf_CResultActor
				, typename tf_CResultFunctor
			>
		bool TCActorInternal<t_CActor>::f_Call
			(
				tf_CFunctor &&_ToCall
				, TCActor<tf_CResultActor> const &_pResultActor
				, tf_CResultFunctor &&_ResultFunctor
			)
		{
			typedef TCReportLocal
				<
					t_CActor
					, typename NTraits::TCMemberFunctionPointerTraits<tf_CFunction>::CReturn
					, typename NTraits::TCRemoveReference<tf_CFunctor>::CType
					, tf_CResultActor
					, tf_CResultFunctor
				>
				CReportLocal
			;
#if 0
			static NThread::CMutual s_Lock;
			static NContainer::TCSet<NStr::CStr> s_Logged;
			{
				DMibLock(s_Lock);
				NStr::CStr ToLog = NStr::fg_Format("ReportLocal: {}  {}", sizeof(CReportLocal), NTraits::TCAlignmentOf<CReportLocal>::mc_Value);
				if (s_Logged(ToLog).f_WasCreated())
					DMibTraceSafe("{}\n", ToLog);
			}
#endif
			this->f_QueueProcess(CReportLocal{fg_Move(_ToCall), fg_Move(_ResultFunctor), _pResultActor, this});
			
			return true; // Dummy return to allow for fg_Swallow on arguments
		}		

		template <>
		template 
			<
				typename tf_CFunction
				, typename tf_CFunctor
				, typename tf_CResultActor
				, typename tf_CResultFunctor
			>
		bool TCActorInternal<CAnyConcurrentActor>::f_Call
			(
				tf_CFunctor &&_ToCall
				, TCActor<tf_CResultActor> const &_pResultActor
				, tf_CResultFunctor &&_ResultFunctor
			)
		{
			return fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Normal).m_pInternalActor->f_Call<tf_CFunction>(_ToCall, _pResultActor, fg_Move(_ResultFunctor));
		}
		
		template <>
		template 
			<
				typename tf_CFunction
				, typename tf_CFunctor
				, typename tf_CResultActor
				, typename tf_CResultFunctor
			>
		bool TCActorInternal<CAnyConcurrentActorLowPrio>::f_Call
			(
				tf_CFunctor &&_ToCall
				, TCActor<tf_CResultActor> const &_pResultActor
				, tf_CResultFunctor &&_ResultFunctor
			)
		{
			return fg_ConcurrencyManager().f_GetConcurrentActorForThisThread(EPriority_Low).m_pInternalActor->f_Call<tf_CFunction>(_ToCall, _pResultActor, fg_Move(_ResultFunctor));
		}		
	}
}
