// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		struct CThisActor;
		template
		<
			bool t_bUnwrapTuple
			, typename t_CHandler
			, typename t_CActor
			, typename t_CResultTypes
			, typename t_CResultIndicies = NMeta::TCConsecutiveIndices<NMeta::TCTypeList_Len<t_CResultTypes>::mc_Value>
		>
		struct TCCallMutipleActorStorage;

		template <typename tf_CActor>
		tf_CActor *fg_GetInternalActor(TCActor<tf_CActor> const &_Actor);

		template <typename tf_CActor>
		tf_CActor *fg_GetInternalActor(TCActorInternal<tf_CActor> const &_Actor);
	}

	struct CActorDistributionManagerInternal;
	struct CCurrentActorScope;
	struct CCurrentlyProcessingActorScope;
	struct CDistributedActorProtocolVersions;

	template <typename t_CActor>
	class align_cacheline TCActorInternal : public t_CActor::CActorHolder
	{
		typedef typename t_CActor::CActorHolder CSuper;

		template <typename t_CActor2>
		friend class TCActor;

		template <typename t_CActor0>
		friend class TCActorInternal;

		friend class CConcurrencyManager;

		template <typename t_CType2, typename... tp_COptions2>
		friend class NStorage::TCSharedPointer;

		friend struct NPrivate::CThisActor;

		friend struct CActorDistributionManagerInternal;

		template
		<
			bool t_bUnwrapTuple
			, typename t_CHandler2
			, typename t_CActor2
			, typename t_CResultTypes2
			, typename t_CResultIndicies2
		>
		friend struct NPrivate::TCCallMutipleActorStorage;

		template <typename tf_CActor>
		friend tf_CActor *NPrivate::fg_GetInternalActor(TCActor<tf_CActor> const &_Actor);

		template <typename tf_CActor>
		friend tf_CActor *NPrivate::fg_GetInternalActor(TCActorInternal<tf_CActor> const &_Actor);

		friend class CActorHolder;
		friend struct CCurrentActorScope;
		friend struct CCurrentlyProcessingActorScope;

		TCActorInternal *fp_GetThis();
		TCActorInternal<CActor> *fp_GetRealActor(NConcurrency::CActorHolder *_pActorInternal) const override;

	public:

		TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData);

		template <typename tf_CP0>
		TCActorInternal(CConcurrencyManager *_pConcurrencyManager, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData, tf_CP0 &&_P0);
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

		template <typename ...tfp_CInterface>
		auto f_Publish(NStr::CStr const &_Namespace, fp32 _WaitForPublicationsTimeout);

		template <typename ...tfp_CInterface>
		auto f_PublishWithVersion(NStr::CStr const &_Namespace, CDistributedActorProtocolVersions const &_Versions, fp32 _WaitForPublicationsTimeout);

		template <typename ...tfp_CInterface>
		auto f_ShareInterface();

		template <typename ...tfp_CInterface>
		auto f_ShareInterfaceWithVersion(CDistributedActorProtocolVersions const &_Versions);

		uint32 f_InterfaceVersion();

	private:
		alignas(t_CActor) uint8 m_ActorMemory[sizeof(t_CActor)];
	};

	template <typename tf_CToDelete>
	void fg_DeleteWeakObject(CInternalActorAllocator &, tf_CToDelete *_pObject)
		requires (NTraits::TCIsBaseOf<tf_CToDelete, NConcurrency::CActorHolder>::mc_Value)
	;
}
