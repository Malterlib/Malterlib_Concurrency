// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			struct CThisActor
			{
				void *m_pThis = nullptr;
				template <typename tf_CMemberFunction, typename... tfp_CCallParams>
				auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
			};
		}
		class CActor
		{
			template <typename tf_CActor>
			friend TCActor<tf_CActor> fg_ThisActor(tf_CActor *_pActor);
			friend class CConcurrencyManager;
			template <typename t_CActor>
			friend class TCActorInternal;
			
			template <typename t_CCallbackSignature, bool _bSupportMultiple, typename t_CExtraData>
			friend class TCActorSubscriptionManager;
			
			friend class CActorHolder;

		protected:
			NPrivate::CThisActor self;
		private:
			CConcurrencyManager *mp_pConcurrencyManager = nullptr;

			void fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);

		protected:
			virtual void f_Construct();
			virtual TCContinuation<void> f_Destroy();
			
		public:
			
			typedef CDefaultActorHolder CActorHolder;
			
			inline_always CConcurrencyManager &f_ConcurrencyManager() const
			{
				return *mp_pConcurrencyManager;
			}
			
			virtual ~CActor();
			void f_Dispatch(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);
			template <typename tf_CReturnType>
			tf_CReturnType f_DispatchWithReturn(NFunction::TCFunctionMovable<tf_CReturnType ()> &&_fToDisptach);
			
			enum
			{
				mc_bAllowInternalAccess = false
				, mc_bImmediateDelete = false
				, mc_Priority = EPriority_Normal
				, mc_bCanBeEmpty = false
			};
		};
		
		class CSeparateThreadActorHolder;
		
		struct CSeparateThreadActor : public CActor
		{
			using CActorHolder = CSeparateThreadActorHolder;
		};

	}
}
