// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_AsyncResult.h"
#include "Malterlib_Concurrency_Continuation.h"
#include "Malterlib_Concurrency_Defines.h"
#include "Malterlib_Concurrency_Actor.h"

namespace NMib
{
	namespace NConcurrency
	{
		template <typename t_CActor>
		struct TCDistributedActorWrapper : public t_CActor
		{
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
