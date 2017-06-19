// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		class CActor;
		namespace NPrivate
		{
			struct CThisActor
			{
				void *m_pThis = nullptr;
				template <typename tf_CMemberFunction, typename... tfp_CCallParams>
				auto operator () (tf_CMemberFunction &&_pMemberFunction, tfp_CCallParams &&... p_CallParams) const;
				operator TCActor<> () const;
			private:
				friend class NConcurrency::CActor;
				
				CThisActor() = default;
				CThisActor(CThisActor &&) = default;
				CThisActor(CThisActor const &) = default;
				CThisActor &operator = (CThisActor &&) = default;
				CThisActor &operator = (CThisActor const &) = default;
			};
		}
		
		class CActor /// \brief Base class for all types used with \ref TCActor
		{
			template <typename tf_CActor>
			friend TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor);
			friend class CConcurrencyManager;
			template <typename t_CActor>
			friend class TCActorInternal;
			
			template <typename t_CCallbackSignature, bool _bSupportMultiple, typename t_CExtraData>
			friend class TCActorSubscriptionManager;
			
			friend class CActorHolder;

		protected:
			NPrivate::CThisActor self;
			CConcurrencyManager *mp_pConcurrencyManager = nullptr;
			bool mp_bDestroyed = false;
			
		private:

			void fp_DisptachInternal(NFunction::TCFunctionMovable<void ()> &&_fToDisptach);

			TCContinuation<void> fp_DestroyInternal();

		protected:
			virtual void fp_Construct();
			virtual TCContinuation<void> fp_Destroy();
			
		public:
			
			typedef CDefaultActorHolder CActorHolder;
			
			inline_always CConcurrencyManager &f_ConcurrencyManager() const
			{
				return *mp_pConcurrencyManager;
			}
			
			CActor();
			CActor(CActor &&) = delete;
			CActor(CActor const &) = delete;
			CActor &operator = (CActor &&) = delete;
			CActor &operator = (CActor const &) = delete;

			bool f_IsDestroyed() const;
			
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
		
		template <typename t_CActor>
		struct TCRoundRobinActors
		{
			TCRoundRobinActors(mint _nActors);
			
			template <typename tf_CParam>
			void f_Construct(tf_CParam &&_Param);
			
			TCContinuation<void> f_Destroy();
			
			TCActor<t_CActor> const &operator *() const;
			
		private:
			NContainer::TCVector<TCActor<t_CActor>> mp_Actors;
			mutable mint mp_iCurrentActor = 0;
		};
	}
}
