// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor)
	{
		TCActorInternal<tf_CActor> *pActor = static_cast<TCActorInternal<tf_CActor> *>(_pActor->self.m_pThis.f_Get());
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCActor<tf_CActor>(fg_Explicit(pActor));
	}

	template <typename tf_CActor>
	TCWeakActor<tf_CActor> fg_ThisActorWeak(tf_CActor const *_pActor)
	{
		TCActorInternal<tf_CActor> *pActor = static_cast<TCActorInternal<tf_CActor> *>(_pActor->self.m_pThis.f_Get());
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCWeakActor<tf_CActor>(TCActorHolderWeakPointer<TCActorInternal<tf_CActor>>(pActor));
	}
}
