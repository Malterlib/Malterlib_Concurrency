// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_RuntimeTypeRegistry.h"
#include "../Actor/Malterlib_Concurrency_Manager.h"

#include <Mib/Cryptography/RandomID>

#include "Malterlib_Concurrency_DistributedActor_Private.hpp"

namespace NMib::NStorage::NPrivate
{
	template <typename t_CActor0, typename t_CActor1, typename t_CAllocator0, typename t_CAllocator1>
	struct TCIsValidConversion<NConcurrency::TCDistributedActorWrapper<t_CActor0>, NConcurrency::TCDistributedActorWrapper<t_CActor1>, t_CAllocator0, t_CAllocator1>
	{
		enum
		{
			mc_Value = TCIsValidConversion<t_CActor0, t_CActor1, t_CAllocator0, t_CAllocator1>::mc_Value
		};
	};

	template <typename t_CActor0, typename t_CActor1, typename t_CAllocator0, typename t_CAllocator1>
	struct TCIsValidConversion<t_CActor0, NConcurrency::TCDistributedActorWrapper<t_CActor1>, t_CAllocator0, t_CAllocator1>
	{
		enum
		{
			mc_Value = TCIsValidConversion<t_CActor0, t_CActor1, t_CAllocator0, t_CAllocator1>::mc_Value
		};
	};
}

namespace NMib::NConcurrency
{
	namespace NPrivate
	{
		template <typename t_CActor>
		struct TCIsDistributedActor
		{
			enum
			{
				mc_Value = false
			};
		};

		template <typename t_CActor>
		struct TCIsDistributedActor<NConcurrency::TCDistributedActorWrapper<t_CActor>>
		{
			enum
			{
				mc_Value = true
			};
		};

		struct CDistributedActorInterfaceInfo
		{
			template <typename ...tfp_CInterface>
			static CDistributedActorInterfaceInfo fs_GenerateInfo();

			template <typename ...tfp_CInterface>
			static CDistributedActorInterfaceInfo fs_GenerateInfo(CDistributedActorProtocolVersions const &_Versions);

			inline_always NContainer::TCVector<uint32> const &f_GetInterfaceHashes() const;
			inline_always CDistributedActorProtocolVersions const &f_GetProtocolVersions() const;

		private:
			CDistributedActorInterfaceInfo();
			NContainer::TCVector<uint32> mp_InterfaceHashes;
			CDistributedActorProtocolVersions mp_ProtocolVersions;
		};

		template <typename tf_CStream>
		NStream::CScopeBinaryStreamVersion ICHost::f_StreamVersion(tf_CStream &_Stream)
		{
			uint32 ProtocolVersion = m_ActorProtocolVersion.f_Load();
			DMibFastCheck(ProtocolVersion != 0);
			return NMib::NStream::CScopeBinaryStreamVersion(_Stream, ProtocolVersion);
		}
	}

	template <typename t_CActor>
	template <typename ...tfp_CInterface>
	auto TCActorInternal<t_CActor>::f_Publish(NStr::CStr const &_Namespace, fp32 _WaitForPublicationsTimeout)
	{
		static_assert(NPrivate::TCIsDistributedActor<t_CActor>::mc_Value, "Must be distributed actor");
		static_assert((NTraits::cIsBaseOfOrSame<t_CActor, tfp_CInterface> && ...), "Trying to publish incompatible interface");

		return NPrivate::fg_PublishActor
			(
				this->template fp_GetAsActor<TCDistributedActorWrapper<CActor>>()
				, NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo<tfp_CInterface...>()
				, _Namespace
				, _WaitForPublicationsTimeout
			)
		;
	}

	template <typename t_CActor>
	template <typename ...tfp_CInterface>
	auto TCActorInternal<t_CActor>::f_ShareInterface()
	{
		static_assert(NPrivate::TCIsDistributedActor<t_CActor>::mc_Value, "Must be distributed actor");
		static_assert((NTraits::cIsBaseOfOrSame<t_CActor, tfp_CInterface> && ...), "Trying to share incompatible interface");

		DMibFastCheck(this);

		auto InterfaceInfo = NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo<tfp_CInterface...>();

		return TCDistributedActorInterfaceShare<NMeta::TCTypeList_Get<0, NMeta::TCTypeList<tfp_CInterface...>>>
			{
				InterfaceInfo.f_GetInterfaceHashes()
				, InterfaceInfo.f_GetProtocolVersions()
				, this->template fp_GetAsActor<TCDistributedActorWrapper<CActor>>()
			}
		;
	}

	template <typename t_CActor>
	template <typename ...tfp_CInterface>
	auto TCActorInternal<t_CActor>::f_PublishWithVersion(NStr::CStr const &_Namespace, CDistributedActorProtocolVersions const &_Versions, fp32 _WaitForPublicationsTimeout)
	{
		static_assert(NPrivate::TCIsDistributedActor<t_CActor>::mc_Value, "Must be distributed actor");
		static_assert((NTraits::cIsBaseOfOrSame<t_CActor, tfp_CInterface> && ...), "Trying to publish incompatible interface");

		return NPrivate::fg_PublishActor
			(
				this->template fp_GetAsActor<TCDistributedActorWrapper<CActor>>()
				, NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo<tfp_CInterface...>(_Versions)
				, _Namespace
				, _WaitForPublicationsTimeout
			)
		;
	}

	template <typename t_CActor>
	template <typename ...tfp_CInterface>
	auto TCActorInternal<t_CActor>::f_ShareInterfaceWithVersion(CDistributedActorProtocolVersions const &_Versions)
	{
		static_assert(NPrivate::TCIsDistributedActor<t_CActor>::mc_Value, "Must be distributed actor");
		static_assert((NTraits::cIsBaseOfOrSame<t_CActor, tfp_CInterface> && ...), "Trying to share incompatible interface");

		auto InterfaceInfo = NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo<tfp_CInterface...>(_Versions);

		return TCDistributedActorInterfaceShare<NMeta::TCTypeList_Get<0, NMeta::TCTypeList<tfp_CInterface...>>>
			{
				InterfaceInfo.f_GetInterfaceHashes()
				, InterfaceInfo.f_GetProtocolVersions()
				, this->template fp_GetAsActor<TCDistributedActorWrapper<CActor>>()
			}
		;
	}

	template <typename t_CActor>
	uint32 TCActorInternal<t_CActor>::f_InterfaceVersion()
	{
		auto pDistributedData = static_cast<NPrivate::CDistributedActorData *>(this->mp_pDistributedActorData.f_Get());
		if (!pDistributedData)
			DMibError("Not a distributed actor");
		return pDistributedData->m_ProtocolVersion;
	}

	template <typename t_CType>
	TCDistributedActorInterfaceShare<t_CType>::TCDistributedActorInterfaceShare
		(
			NContainer::TCVector<uint32> const &_Hierarchy
			, CDistributedActorProtocolVersions const &_ProtocolVersions
			, TCDistributedActor<CActor> &&_Actor
		)
		: CDistributedActorInterfaceShare(_Hierarchy, _ProtocolVersions, fg_Move(_Actor))
	{
	}

	template <typename t_CType>
	TCDistributedActor<t_CType> TCDistributedActorInterfaceShare<t_CType>::f_GetActor() const
	{
		return fg_StaticCast<TCDistributedActorWrapper<t_CType>>(m_Actor);
	}

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<TCDistributedActorWrapper<tf_CActor>> CActorDistributionManagerHolder::f_ConstructActor(tfp_CParams &&...p_Params)
	{
		return fg_ConstructDistributedActor<tf_CActor>(fp_GetAsActor<CActorDistributionManager>(), fg_Forward<tfp_CParams>(p_Params)...);
	}

	template <typename tf_CActor, typename tf_CDelegateTo, typename... tfp_CParams>
	TCActor<TCDistributedActorWrapper<TCDistributedInterfaceDelegator<tf_CActor, tf_CDelegateTo>>> CActorDistributionManagerHolder::f_ConstructInterface
		(
			tf_CDelegateTo *_pDelegateTo
			, tfp_CParams &&...p_Params
		)
	{
		return fg_ConstructDistributedActor<TCDistributedInterfaceDelegator<tf_CActor, tf_CDelegateTo>>
			(
				fp_GetAsActor<CActorDistributionManager>()
				, fg_Construct(fg_ThisActor(_pDelegateTo))
				, _pDelegateTo
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename t_CInterface, typename t_CDelegateTo>
	template <typename ...tfp_CParams>
	TCDistributedInterfaceDelegator<t_CInterface, t_CDelegateTo>::TCDistributedInterfaceDelegator(t_CDelegateTo *_pDelegateTo, tfp_CParams && ...p_Params)
		: t_CInterface(fg_Forward<tfp_CParams>(p_Params)...)
	{
#ifdef DMibDebug
		static_assert(NTraits::cIsSame<decltype(&t_CInterface::self), CEmpty (t_CInterface::*)>);
//		If you get this static assert, add a dummy self like below:
//
//		DDelegatedActorImplementation(CMyActor);
//
#endif

		t_CInterface::m_pThis = _pDelegateTo;
	}

	template <typename t_CImplementation>
	TCDistributedActorInstance<t_CImplementation>::~TCDistributedActorInstance()
	{
#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();

		// The lifetime of this object needs to be tied to the containing actor
		DMibFastCheck(!mp_pThis || ThreadLocal.m_pCurrentlyDestructingActor == mp_pThis);
#endif
	}

	template <typename t_CImplementation>
	TCUnsafeFuture<void> TCDistributedActorInstance<t_CImplementation>::f_Destroy()
	{
		if (!m_pActor)
			co_return {};

#if DMibEnableSafeCheck > 0
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		DMibFastCheck(ThreadLocal.m_pCurrentlyProcessingActorHolder == mp_pThis->self.m_pThis.f_Get());
#endif
		m_pActor = nullptr;

		co_await m_Publication.f_Destroy();

		if (!m_Actor.f_IsEmpty())
			co_await fg_Move(m_Actor).f_Destroy();

		co_return {};
	}

	template <typename t_CImplementation>
	template <typename tf_CFirstInterface, typename ...tfp_CInterfaces, typename tf_CThis>
	TCUnsafeFuture<void> TCDistributedActorInstance<t_CImplementation>::f_Publish
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, tf_CThis *_pThis
			, fp32 _WaitForPublicationsTimeout
			, NStr::CStr const &_Namespace
		)
	{
		if (!m_Actor)
			f_Construct(_DistributionManager, _pThis);

		m_Publication = co_await m_Actor->template f_Publish<tf_CFirstInterface, tfp_CInterfaces...>(_Namespace, _WaitForPublicationsTimeout);

		co_return {};
	}

	template <typename t_CImplementation>
	template <typename tf_CThis>
	void TCDistributedActorInstance<t_CImplementation>::f_Construct(TCActor<CActorDistributionManager> const &_DistributionManager, tf_CThis *_pThis)
	{
#if DMibEnableSafeCheck > 0
		DMibFastCheck(!mp_pThis);
		DMibFastCheck(!m_Actor);
		auto &ThreadLocal = fg_ConcurrencyThreadLocal();
		DMibFastCheck(ThreadLocal.m_pCurrentlyProcessingActorHolder == _pThis->self.m_pThis.f_Get());
		mp_pThis = _pThis;
#endif

		auto Interface = _DistributionManager->f_ConstructInterface<t_CImplementation>(_pThis);
		m_pActor = &Interface->f_AccessInternal();
		m_Actor = fg_Move(Interface);
	}

	template <typename ...tfp_CInterface>
	NPrivate::CDistributedActorInterfaceInfo NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo(CDistributedActorProtocolVersions const &_Versions)
	{
		CDistributedActorInterfaceInfo Ret;

		(
			[&]
			{
				Ret.mp_InterfaceHashes.f_Insert(DMibConstantTypeHash(tfp_CInterface));
			}
			()
			, ...
		);

		Ret.mp_ProtocolVersions = _Versions;

		return Ret;
	}

	template <typename ...tfp_CInterface>
	NPrivate::CDistributedActorInterfaceInfo NPrivate::CDistributedActorInterfaceInfo::fs_GenerateInfo()
	{
		CDistributedActorInterfaceInfo Ret;

		CDistributedActorProtocolVersions Versions;

#if DMibEnableSafeCheck > 0
		bool bMultipleVersions = false;
#endif

		(
			[&]
			{
				CDistributedActorProtocolVersions ThisVersions{tfp_CInterface::EProtocolVersion_Min, tfp_CInterface::EProtocolVersion_Current};
				if (Versions.m_MinSupported == TCLimitsInt<uint32>::mc_Max)
					Versions = ThisVersions;
#if DMibEnableSafeCheck > 0
				else if (ThisVersions != Versions)
					bMultipleVersions = true;
#endif
				Ret.mp_InterfaceHashes.f_Insert(DMibConstantTypeHash(tfp_CInterface));
			}
			()
			, ...
		);

		DMibFastCheck(!bMultipleVersions); // Can only publish one protocol version

		Ret.mp_ProtocolVersions = Versions;

		return Ret;
	}

	NContainer::TCVector<uint32> const &NPrivate::CDistributedActorInterfaceInfo::f_GetInterfaceHashes() const
	{
		return mp_InterfaceHashes;
	}

	CDistributedActorProtocolVersions const &NPrivate::CDistributedActorInterfaceInfo::f_GetProtocolVersions() const
	{
		return mp_ProtocolVersions;
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActorSource>>> const &_pActorHolder)
	{
		using CActor = typename TCIsDistributedActorWrapper<tf_CActor>::CActor;

		auto pDummy = static_cast<CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> const &>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActorSource>>> &&_pActorHolder)
	{
		using CActor = typename TCIsDistributedActorWrapper<tf_CActor>::CActor;

		auto pDummy = static_cast<CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderSharedPointer<TCActorInternal<tf_CActor>> &&>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderWeakPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActorSource>>> const &_pActorHolder)
	{
		using CActor = typename TCIsDistributedActorWrapper<tf_CActor>::CActor;

		auto pDummy = static_cast<CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderWeakPointer<TCActorInternal<tf_CActor>> const &>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorHolderWeakPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActorSource>>> &&_pActorHolder)
	{
		using CActor = typename TCIsDistributedActorWrapper<tf_CActor>::CActor;

		auto pDummy = static_cast<CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder.f_Get());
		else
			return reinterpret_cast<TCActorHolderWeakPointer<TCActorInternal<tf_CActor>> &&>(_pActorHolder);
	}

	template <typename tf_CActor, typename tf_CActorSource>
	decltype(auto) fg_StaticCast(TCActorInternal<TCDistributedActorWrapper<tf_CActorSource>> *_pActorHolder)
	{
		using CActor = typename TCIsDistributedActorWrapper<tf_CActor>::CActor;

		auto pDummy = static_cast<CActor *>((tf_CActorSource *)nullptr);
		(void)pDummy;

		if constexpr (TCIsActorAlwaysAlive<CActor>::mc_bImpl)
			return reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder);
		else
			return TCActorHolderSharedPointer<TCActorInternal<tf_CActor>>(fg_Explicit(reinterpret_cast<TCActorInternal<tf_CActor> *>(_pActorHolder)));
	}

	template <typename tf_CFirst>
	NContainer::TCVector<uint32> fg_GetDistributedActorInheritanceHierarchy()
	{
		NContainer::TCVector<uint32> Return;

		NPrivate::fg_GetDistributedActorInheritanceHierarchy<tf_CFirst>(Return);

		return Return;
	}

	template <typename t_CActor>
	template <typename... tf_CActor>
	TCDistributedActorWrapper<t_CActor>::TCDistributedActorWrapper(tf_CActor &&...p_Params)
		: t_CActor(fg_Forward<tf_CActor>(p_Params)...)
	{
	}

	template <typename tf_CActor, typename... tfp_CParams>
	TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(TCActor<CActorDistributionManager> const &_DistributionManager, tfp_CParams &&...p_Params)
	{
		auto &ConcurrencyManager = _DistributionManager->f_ConcurrencyManager();

		NStorage::TCSharedPointer<NPrivate::CDistributedActorData> pDistributedActorData = fg_Construct();
		pDistributedActorData->m_ActorID = NCryptography::fg_RandomID();
		pDistributedActorData->m_DistributionManager = _DistributionManager;

		TCActorHolderSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActor>>> pActor
			= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData))
		;

		return ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<tf_CActor>>
			(
				fg_Move(pActor)
				, fg_Construct<TCDistributedActorWrapper<tf_CActor>>(fg_Forward<tfp_CParams>(p_Params)...)
			)
		;
	}

	template <typename tf_CActor, typename... tfp_CParams>
	TCDistributedActor<tf_CActor> fg_ConstructRemoteDistributedActor
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, NStr::CStr const &_ActorID
			, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
			, uint32 _ProtocolVersion
			, tfp_CParams &&...p_Params
		)
	{
		NStorage::TCSharedPointer<NPrivate::CDistributedActorData> pDistributedActorData = fg_Construct();
		pDistributedActorData->m_DistributionManager = _DistributionManager;
		pDistributedActorData->m_ActorID = _ActorID;
		pDistributedActorData->m_pHost = _pHost;
		pDistributedActorData->m_ProtocolVersion = _ProtocolVersion;
		pDistributedActorData->m_bRemote = true;
		DMibFastCheck(_ProtocolVersion != TCLimitsInt<uint32>::mc_Max);

		auto &ConcurrencyManager = _DistributionManager->f_ConcurrencyManager();
		TCActorHolderSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActor>>> pActor = fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData));
		return ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<tf_CActor>>(fg_Move(pActor), fg_Construct<TCDistributedActorWrapper<tf_CActor>>());
	}

#ifndef DCompiler_MSVC_Workaround
	extern template TCDistributedActor<CActor> fg_ConstructRemoteDistributedActor<CActor>
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, NStr::CStr const &_ActorID
			, NStorage::TCWeakPointer<NPrivate::ICHost> const &_pHost
			, uint32 _ProtocolVersion
		)
	;
#endif

	namespace NPrivate
	{
		template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams, mint... tfp_Indidies>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActorHelper
			(
				TCActor<CActorDistributionManager> const &_DistributionManager
				, TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams
				, NMeta::TCIndices<tfp_Indidies...> const &
				, tfp_CParams &&...p_Params
			)
		{
			auto &ConcurrencyManager = _DistributionManager->f_ConcurrencyManager();

			NStorage::TCSharedPointer<NPrivate::CDistributedActorData> pDistributedActorData = fg_Construct();
			pDistributedActorData->m_ActorID = NCryptography::fg_RandomID();
			pDistributedActorData->m_DistributionManager = _DistributionManager;

			TCActorHolderSharedPointer<TCActorInternal<TCDistributedActorWrapper<tf_CActor>>> pActor
				= fg_Construct(&ConcurrencyManager, fg_Move(pDistributedActorData), fg_Forward<tfp_CHolderParams>(fg_Get<tfp_Indidies>(_HolderParams.m_Params))...)
			;

			return ConcurrencyManager.f_ConstructFromInternalActor<TCDistributedActorWrapper<tf_CActor>>
				(
					fg_Move(pActor)
					, fg_Construct<TCDistributedActorWrapper<tf_CActor>>(fg_Forward<tfp_CParams>(p_Params)...)
				)
			;
		}
	}

	template <typename tf_CActor, typename tf_CHolderType, typename... tfp_CHolderParams, typename... tfp_CParams>
	TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor
		(
			TCActor<CActorDistributionManager> const &_DistributionManager
			, TCConstruct<tf_CHolderType, tfp_CHolderParams...> &&_HolderParams
			, tfp_CParams &&...p_Params
		)
	{
		return NPrivate::fg_ConstructDistributedActorHelper<tf_CActor>
			(
				_DistributionManager
				, fg_Move(_HolderParams)
				, NMeta::TCConsecutiveIndices<sizeof...(tfp_CHolderParams)>()
				, fg_Forward<tfp_CParams>(p_Params)...
			)
		;
	}

	template <typename tf_CStream>
	void CActorDistributionCryptographySettings::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_PrivateKey;
		_Stream << m_PublicCertificate;
		_Stream << m_RemoteClientCertificates;
		_Stream << m_SignedClientCertificates;
		_Stream << m_Serial;
		_Stream << m_Subject;
	}

	template <typename tf_CStream>
	void CActorDistributionCryptographySettings::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PrivateKey;
		_Stream >> m_PublicCertificate;
		_Stream >> m_RemoteClientCertificates;
		_Stream >> m_SignedClientCertificates;
		_Stream >> m_Serial;
		_Stream >> m_Subject;
	}

	template <typename tf_CStream>
	void CActorDistributionCryptographyRemoteServer::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_PublicServerCertificate;
		_Stream << m_PublicClientCertificate;
	}

	template <typename tf_CStream>
	void CActorDistributionCryptographyRemoteServer::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_PublicServerCertificate;
		_Stream >> m_PublicClientCertificate;
	}

	template <typename tf_CType>
	TCDistributedActor<tf_CType> CAbstractDistributedActor::f_GetActor() const
	{
		auto Actor = f_GetActor(DMibConstantTypeHash(tf_CType), CDistributedActorProtocolVersions{tf_CType::EProtocolVersion_Min, tf_CType::EProtocolVersion_Current});
		return fg_StaticCast<TCDistributedActorWrapper<tf_CType>>(fg_Move(Actor));
	}

	inline NStr::CStr const &CAbstractDistributedActor::f_GetUniqueHostID() const
	{
		return mp_UniqueHostID;
	}

	inline NStr::CStr const &CAbstractDistributedActor::f_GetRealHostID() const
	{
		return mp_HostInfo.m_HostID;
	}

	inline CHostInfo const &CAbstractDistributedActor::f_GetHostInfo() const
	{
		return mp_HostInfo;
	}

	template <typename tf_CStr>
	void CDistributedActorIdentifier::f_Format(tf_CStr &o_Str) const
	{
		o_Str += typename tf_CStr::CFormat("HostID {} ActorID {}") << mp_pHost << mp_ActorID;
	}

	inline bool CAbstractDistributedActor::f_IsEmpty() const
	{
		return mp_ActorID.f_IsEmpty();
	}

	CDistributedActorProtocolVersions const &CAbstractDistributedActor::f_GetProtocolVersions() const
	{
		return mp_ProtocolVersions;
	}

	template <typename tf_CString>
	void CHostInfo::f_Format(tf_CString &o_String) const
	{
		o_String += f_GetDesc();
	}

	template <typename tf_CType>
	CDistributedActorProtocolVersions fg_SubscribeVersions(uint32 _MinSupportedVersion, uint32 _MaxSupportedVersion)
	{
		return CDistributedActorProtocolVersions{fg_Max(uint32(tf_CType::EProtocolVersion_Min), _MinSupportedVersion), fg_Min(uint32(tf_CType::EProtocolVersion_Current), _MaxSupportedVersion)};
	}

	template <typename t_CActor>
	bool CDistributedActorIdentifier::operator == (TCActor<t_CActor> const &_Right) const noexcept
	{
		return *this == (TCActor<> const &)_Right;
	}

	template <typename t_CActor>
	COrdering_Weak CDistributedActorIdentifier::operator <=> (TCActor<t_CActor> const &_Right) const noexcept
	{
		return *this <=> (TCActor<> const &)_Right;
	}

	template <typename tf_CStream>
	void CDistributedActorProtocolVersions::f_Feed(tf_CStream &_Stream) const
	{
		_Stream << m_MinSupported;
		_Stream << m_MaxSupported;
	}

	template <typename tf_CStream>
	void CDistributedActorProtocolVersions::f_Consume(tf_CStream &_Stream)
	{
		_Stream >> m_MinSupported;
		_Stream >> m_MaxSupported;
	}

	template <typename tf_CActor>
	NStr::CStr fg_GetRemoteActorID(TCDistributedActor<tf_CActor> const &_NewActor)
	{
		auto pDistributedActor = _NewActor->f_GetDistributedActorData().f_Get();
		if (!pDistributedActor)
			return {};
		return pDistributedActor->m_ActorID;
	}

	template <typename tf_CActor>
	NStr::CStr fg_GetRemoteActorID(TCWeakDistributedActor<tf_CActor> const &_NewActor)
	{
		auto Actor = _NewActor.f_Lock();
		if (!Actor)
			return {};
		return fg_GetRemoteActorID(Actor);
	}

	template <typename tf_CStream>
	void CActorDistributionManager::CWebsocketDebugStats::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_nSentBytes;
		_Stream % m_nReceivedBytes;
		_Stream % m_IncomingDataBufferBytes;
		_Stream % m_OutgoingDataBufferBytes;
		_Stream % m_SecondsSinceLastSend;
		_Stream % m_SecondsSinceLastReceive;
		_Stream % m_State;
	}

	template <typename tf_CStream>
	void CActorDistributionManager::CConnectionDebugStats::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_URL;
		_Stream % m_WebsocketStats;
		_Stream % m_bIncoming;
		_Stream % m_bIdentified;
	}

	template <typename tf_CStream>
	void CActorDistributionManager::CDebugHostStats_PriorityQueue::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Incoming_NextPacketID;
		_Stream % m_Incoming_PacketsQueueLength;
		_Stream % m_Incoming_PacketsQueueBytes;
		_Stream % m_Incoming_PacketsQueueIDs;

		_Stream % m_Outgoing_CurrentPacketID;
		_Stream % m_Outgoing_PacketsQueueLength;
		_Stream % m_Outgoing_PacketsQueueBytes;
		_Stream % m_Outgoing_PacketsQueueIDs;

		_Stream % m_Outgoing_SentPacketsQueueLength;
		_Stream % m_Outgoing_SentPacketsQueueBytes;
		_Stream % m_Outgoing_SentPacketsQueueIDs;
	}

	template <typename tf_CStream>
	void CActorDistributionManager::CDebugHostStats::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_HostID;
		_Stream % m_UniqueHostID;
		_Stream % m_Connections;

		if (_Stream.f_SupportsVersion(CConnectionsDebugStats::EVersion::mc_AddPrioritization))
			_Stream % m_PriorityQueues;
		else
		{
			if constexpr (tf_CStream::mc_bConsume)
			{
				auto &DefaultQueue = m_PriorityQueues[128u];
				_Stream >> DefaultQueue.m_Incoming_PacketsQueueLength;
				_Stream >> DefaultQueue.m_Incoming_NextPacketID;
				_Stream >> DefaultQueue.m_Outgoing_PacketsQueueLength;
				_Stream >> DefaultQueue.m_Outgoing_SentPacketsQueueLength;
				_Stream >> DefaultQueue.m_Outgoing_CurrentPacketID;
			}
			else
			{
				CDebugHostStats_PriorityQueue DefaultStats;
				auto *pQueue = m_PriorityQueues.f_FindEqual(128u);
				if (!pQueue)
					pQueue = &DefaultStats;

				auto &DefaultQueue = *pQueue;

				_Stream << DefaultQueue.m_Incoming_PacketsQueueLength;
				_Stream << DefaultQueue.m_Incoming_NextPacketID;
				_Stream << DefaultQueue.m_Outgoing_PacketsQueueLength;
				_Stream << DefaultQueue.m_Outgoing_SentPacketsQueueLength;
				_Stream << DefaultQueue.m_Outgoing_CurrentPacketID;
			}
		}

		_Stream % m_nSentPackets;
		_Stream % m_nSentBytes;

		_Stream % m_nReceivedPackets;
		_Stream % m_nReceivedBytes;

		_Stream % m_nDiscardedPackets;
		_Stream % m_nDiscardedBytes;

		_Stream % m_LastExecutionID;
		_Stream % m_ExecutionID;
		_Stream % m_FriendlyName;
		_Stream % m_LastError;
		_Stream % m_LastErrorTime;
	}

	template <typename tf_CStream>
	void CActorDistributionManager::CConnectionsDebugStats::f_Stream(tf_CStream &_Stream)
	{
		_Stream % m_Hosts;
	}
}

#include "Malterlib_Concurrency_DistributedActor_Stream.hpp"
