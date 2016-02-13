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
		}
		template <typename t_CActor>
		class TCActorInternal : public t_CActor::CActorHolder
		{
			typedef typename t_CActor::CActorHolder CSuper;

			template <typename t_CActor0>
			friend class TCActorInternal;

			friend class CConcurrencyManager;
			
			template <typename t_CType2, typename... tp_COptions2>
			friend class NPtr::TCSharedPointer;
			
			friend struct NPrivate::CThisActor;
			
			template 
			<
				typename t_CActor2
				, typename t_CRet
				, typename t_CFunctor
				, typename t_CResultActor
				, typename t_CResultFunctor
			>
			friend struct TCReportLocal;


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
			
			friend class CActorHolder;
#if 0
#ifdef DCompiler_clang
			template <typename tf_CActor, typename tf_CFunctor>
			friend void CActorHolder::f_Destroy(TCActorResultCall<tf_CActor, tf_CFunctor> const &_ResultCall);
#endif
#endif
			
			t_CActor *fp_GetActor() const;
			
		public:

			TCActorInternal(CConcurrencyManager *_pConcurrencyManager);
			template <typename tf_CP0>
			TCActorInternal(CConcurrencyManager *_pConcurrencyManager, tf_CP0 &&_P0);
			~TCActorInternal();

		public:

			enum
			{
				mc_bAllowInternalAccess = t_CActor::mc_bAllowInternalAccess
			};

			t_CActor &f_AccessInternal();
			t_CActor const &f_AccessInternal() const;

			template 
				<
					typename tf_CRet
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

		};

		template <typename tf_CToDelete>
		auto fg_DeleteWeakObject(CInternalActorAllocator& _Allocator, tf_CToDelete *_pObject)
			-> typename TCEnableIf<NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value, void>::CType
		;

	}
}
