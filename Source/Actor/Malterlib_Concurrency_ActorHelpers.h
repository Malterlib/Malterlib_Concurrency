// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename tf_CActor>
	DMibSuppressUndefinedSanitizer TCActor<tf_CActor> fg_ThisActor(tf_CActor const *_pActor)
	{
		TCActorInternal<tf_CActor> *pActor = static_cast<TCActorInternal<tf_CActor> *>(_pActor->self.m_pThis.f_Get());
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCActor<tf_CActor>(fg_Explicit(pActor));
	}

	template <typename tf_CActor>
	TCActor<tf_CActor> fg_ThisActor(TCActorInternal<tf_CActor> *_pActor)
	{
		return TCActor<tf_CActor>(fg_Explicit(_pActor));
	}

	inline_always TCActor<CActor> fg_ThisActor(CActorHolder *_pActor)
	{
		return TCActor<CActor>(fg_Explicit(static_cast<TCActorInternal<CActor> *>(_pActor)));
	}

	template <typename tf_CActor>
	DMibSuppressUndefinedSanitizer TCWeakActor<tf_CActor> fg_ThisActorWeak(tf_CActor const *_pActor)
	{
		TCActorInternal<tf_CActor> *pActor = static_cast<TCActorInternal<tf_CActor> *>(_pActor->self.m_pThis.f_Get());
		DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
		return TCWeakActor<tf_CActor>(TCActorHolderWeakPointer<TCActorInternal<tf_CActor>>(pActor));
	}
}
