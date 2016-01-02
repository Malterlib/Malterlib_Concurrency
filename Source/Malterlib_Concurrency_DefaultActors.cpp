// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{
		void CConcurrentActor::f_Call()
		{
		}
		void CConcurrentActor::f_Dispatch(NFunction::TCFunction<void ()> && _ToDispatch)
		{
			_ToDispatch();
		}
	}
}
