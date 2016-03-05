// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>

namespace NMib
{
	namespace NConcurrency
	{
		CActor::~CActor()
		{
		}

		void CActor::fp_DisptachInternal(NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fToDisptach)
		{
			_fToDisptach();
		}
	
		void CActor::f_Dispatch(NFunction::TCFunction<void (NFunction::CThisTag &), NFunction::CFunctionNoCopyTag> &&_fToDisptach)
		{
			_fToDisptach();
		}
	
		void CActor::f_Construct()
		{
		}
		
		TCContinuation<void> CActor::f_Destroy()
		{
			return TCContinuation<void>::fs_Finished();
		}
	}
}

