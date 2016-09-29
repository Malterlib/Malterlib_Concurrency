// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{

		template <typename tf_CActor>
		TCActor<tf_CActor> fg_ThisActor(tf_CActor *_pActor)
		{
			TCActorInternal<tf_CActor> *pActor = (TCActorInternal<tf_CActor> *)_pActor->self.m_pThis;
			DMibRequire(pActor)("Actor not yet fully constructed, override f_Construct instead");
			return TCActor<tf_CActor>(fg_Explicit(pActor));
		}
		
	}
}
