// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_AsyncResult.h"
#include "Malterlib_Concurrency_Continuation.h"
#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Actor.h"

#include <Mib/Web/HTTP/URL>

namespace NMib
{
	namespace NConcurrency
	{
		class CDistributedActor : public CActor
		{
			NContainer::TCVector<uint32> mp_InheritanceHierarchy;
		};

		template <typename t_CActor>
		struct TCDistributedActorWrapper : public t_CActor
		{
			static_assert(NTraits::TCIsBaseOf<t_CActor, CDistributedActor>::mc_Value, "Need to be base of CDistributedActor");
			enum
			{
				mc_bAllowInternalAccess = true
			};
			
			template <typename... tf_CActor>
			TCDistributedActorWrapper(tf_CActor &&...p_Params);
		};
		
		template <typename t_CActor>
		using TCDistributedActor = TCActor<TCDistributedActorWrapper<t_CActor>>;

		template <typename tf_CActor, typename... tfp_CParams>
		TCActor<TCDistributedActorWrapper<tf_CActor>> fg_ConstructDistributedActor(tfp_CParams &&...p_Params);
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
		auto fg_CallActor(tf_CActor const &_Actor, tfp_CParams && ...p_Params)
		{
			return _Actor(t_pMemberFunction, fg_Forward<tfp_CParams>(p_Params)...);
		}
		
		template <typename tf_CMemberFunction, tf_CMemberFunction t_pMemberFunction, uint32 t_NameHash, typename tf_CActor, typename... tfp_CParams>
		auto fg_CallActor(TCActor<TCDistributedActorWrapper<tf_CActor>> const &_Actor, tfp_CParams && ...p_Params);
		
		struct CActorDistributionHost
		{
			NStr::CStr m_Name;
			NStr::CStr m_UniqueID;
		};

		struct CActorDistributionConnectionSettings
		{
			NHTTP::CURL m_ServerURL;
			NContainer::TCVector<uint8> m_Certificate;
			
		};

		struct CActorDistributionListenSettings
		{
			NNet::CNetAddress m_ListenAddress;
			NContainer::TCVector<uint8> m_Certificate;

			CActorDistributionListenSettings(uint16 _Port)
			{
				NNet::CNetAddressTCPv4 AnyAddress;
				AnyAddress.m_Port = _Port;
				m_ListenAddress = AnyAddress;
			}
			
			CActorDistributionListenSettings(NNet::CNetAddress const &_Address)
				: m_ListenAddress(_Address)
			{
			}
			
			static CActorDistributionConnectionSettings fs_GenerateNewSettings(NHTTP::CURL const &_URL);
		};

		struct CAbstractDistributedActor
		{
			template <typename tf_CType>
			TCDistributedActor<tf_CType> f_GetActor() const
			{
				uint32 Hash = fg_GetTypeHash<tf_CType>();
				if (mp_InheritanceHierarchy.f_BinarySearch(Hash) < 0)
				{
					DMibFastCheck(false);
					return TCDistributedActor<tf_CType>();
				}
				return TCDistributedActor<tf_CType>(mp_Actor);
			}
		private:
			TCDistributedActor<CActor> mp_Actor;
			NContainer::TCVector<uint32> mp_InheritanceHierarchy;
		};
		
		struct CActorDistributionManager : public CActor
		{
			void f_Connect(CActorDistributionConnectionSettings const &_Setting);
			void f_Listen(CActorDistributionConnectionSettings const &_Setting);
			
			void f_PublishActor(TCDistributedActor<CActor> &&_Actor, NContainer::TCVector<NStr::CStr> const &_NameSpaces);
			
			CActorCallback f_SubscribeActors
				(
					NContainer::TCVector<NStr::CStr> const &_NameSpaces
					, TCActor<CActor> const &_Actor
					, NFunction::TCFunction<void (NFunction::CThisTag &, CAbstractDistributedActor &&_NewActor, CActorDistributionHost const &_Host)> &&_fOnNewActor
				)
			;
		};
		
		TCActor<CActorDistributionManager> const &f_GetDistributionManager();
		
#define DMibCallActor(d_Actor, d_Function, d_Args...) ::NMib::NConcurrency::fg_CallActor<decltype(&d_Function), &d_Function, fg_GetMemberFunctionHash<decltype(&d_Function)>(DMibStringize(d_Function))>(d_Actor, ##d_Args)
		
#ifndef DMibPNoShortCuts
#	define DCallActor DMibCallActor
#endif		
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActor.hpp"
