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
			friend class TCActorCallbackManager;
			
			friend class CActorHolder;

		protected:
			NPrivate::CThisActor self;
		private:
			CConcurrencyManager *m_pConcurrencyManager = nullptr;

			void fp_DisptachInternal(NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fToDisptach);

		protected:
			virtual void f_Construct();
			virtual TCContinuation<void> f_Destroy();
			
		public:
			typedef CDefaultActorHolder CActorHolder;
			
			virtual ~CActor();
			void f_Dispatch(NFunction::TCFunction<void (NFunction::CThisTag &)> &&_fToDisptach);
			template <typename tf_CReturnType>
			tf_CReturnType f_DispatchWithReturn(NFunction::TCFunction<tf_CReturnType (NFunction::CThisTag &)> &&_fToDisptach);
			
			enum
			{
				mc_bConcurrent = false
				, mc_bAllowInternalAccess = false
				, mc_bImmediateDelete = false
				, mc_Priority = EPriority_Normal
			};
		};
		
		class CSeparateThreadActorHolder;
		
		struct CSeparateThreadActor : public CActor
		{
			using CActorHolder = CSeparateThreadActorHolder;
		};

	}
}
