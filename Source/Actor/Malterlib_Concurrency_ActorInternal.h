// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{
		namespace NPrivate
		{
			struct CThisActor;
			template 
				<
					typename t_CHandler
					, typename t_CActor
					, typename t_CResultTypes
					, typename t_CResultIndicies = typename NMeta::TCMakeConsecutiveIndices<NMeta::TCTypeList_Len<t_CResultTypes>::mc_Value>::CType
				>
			struct TCCallMutipleActorStorage;
			template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultActor, typename tf_CResultFunctor>
				bool fg_CallActorInternal
				(
					TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList> &_ActorCall
					, TCActor<tf_CResultActor> &&_ResultActor
					, tf_CResultFunctor &&_fResultFunctor
				)
			;
		}
		
		struct CActorDistributionManagerInternal;
		
		template <typename t_CActor>
		class TCActorInternal : public t_CActor::CActorHolder
		{
			typedef typename t_CActor::CActorHolder CSuper;

			template <typename t_CActor2>
			friend class TCActor;
			
			template <typename t_CActor0>
			friend class TCActorInternal;

			friend class CConcurrencyManager;
			
			template <typename t_CType2, typename... tp_COptions2>
			friend class NPtr::TCSharedPointer;
			
			friend struct NPrivate::CThisActor;
			
			friend struct CActorDistributionManagerInternal;
			
			template 
			<
				typename t_CActor2
				, typename t_CRet
				, typename t_CFunctor
				, typename t_CResultActor
				, typename t_CResultFunctor
			>
			friend struct TCReportLocal;

			template 
				<
					typename t_CHandler2
					, typename t_CActor2
					, typename t_CResultTypes2
					, typename t_CResultIndicies2
				>
			friend struct NPrivate::TCCallMutipleActorStorage;
			

			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				friend typename TCEnableIf
				<
					!NPrivate::TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					&& !NTraits::TCIsSame<tf_CResult, TCAsyncResult<void>>::mc_Value
					, void
				>::CType 
				NPrivate::fg_CallWithAsyncResult(tf_CLocal &_Local)
			;

			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				friend typename TCEnableIf
				<
					NPrivate::TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					, void
				>::CType 
				NPrivate::fg_CallWithAsyncResult(tf_CLocal &_Local)
			;

			template <typename tf_CResult, typename tf_CToCall, typename tf_CArgument, typename tf_CLocal>
				friend typename TCEnableIf
				<
					!NPrivate::TCIsContinuation
					<
						typename NTraits::TCIsCallableWith
						<
							typename NTraits::TCRemoveReference<tf_CToCall>::CType
							, void (tf_CArgument &&)
						>::CReturnType
					>::mc_Value
					&& NTraits::TCIsSame<tf_CResult, TCAsyncResult<void>>::mc_Value
					, void
				>::CType 
				NPrivate::fg_CallWithAsyncResult(tf_CLocal &_Local)
			;
			
			template <typename tf_CActor, typename tf_CFunctor, typename tf_CParams, typename tf_CTypeList, typename tf_CResultActor, typename tf_CResultFunctor>
				friend bool NPrivate::fg_CallActorInternal
				(
					TCActorCall<TCWeakActor<tf_CActor>, tf_CFunctor, tf_CParams, tf_CTypeList> &_ActorCall
					, TCActor<tf_CResultActor> &&_ResultActor
					, tf_CResultFunctor &&_fResultFunctor
				)
			;
			
			
			friend class CActorHolder;
			
			t_CActor *fp_GetActor() const;
			
		public:

			TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);
			
			template <typename tf_CP0>
			TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NPtr::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData, tf_CP0 &&_P0);
			~TCActorInternal();

		public:

			enum
			{
				mc_bAllowInternalAccess = t_CActor::mc_bAllowInternalAccess
			};

			inline_always t_CActor &f_AccessInternal();
			inline_always t_CActor const &f_AccessInternal() const;

			inline_always CConcurrencyManager &f_ConcurrencyManager() const;
			inline_always EPriority f_GetPriority() const;

			template 
				<
					typename tf_CFunction
					, typename tf_CFunctor
					, typename tf_CResultActor
					, typename tf_CResultFunctor
				>
			bool f_Call
				(
					tf_CFunctor &&_ToCall
					, TCActor<tf_CResultActor> const &_pResultActor
					, tf_CResultFunctor &&_ResultFunctor
				)
			;
			
			template <typename ...tfp_CInterface>
			auto f_Publish(NStr::CStr const &_Namespace);

			template <typename ...tfp_CInterface>
			auto f_ShareInterface();
			
		private:
			typename NTraits::TCAlign<uint8 [sizeof(t_CActor)], NTraits::TCAlignmentOf<t_CActor>::mc_Value>::CType m_ActorMemory;
		};

		template <typename tf_CToDelete>
		auto fg_DeleteWeakObject(CInternalActorAllocator& _Allocator, tf_CToDelete *_pObject)
			-> typename TCEnableIf<NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value, void>::CType
		;

	}
}
