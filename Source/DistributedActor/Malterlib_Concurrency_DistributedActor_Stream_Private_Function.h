// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency::NPrivate
{
	struct CStreamingFunction : public NPtr::TCSharedPointerIntrusiveBase<>
	{
		virtual ~CStreamingFunction();
		virtual NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call(CDistributedActorReadStream &_Stream) = 0;
		virtual bool f_IsEmpty() const = 0;
	};
	
	template <typename t_FFunction, typename t_FFunctionSignature = typename NFunction::TCFunctionInfo<t_FFunction>::template TCCallType<0>>
	struct TCStreamingFunction : public CStreamingFunction
	{
		TCStreamingFunction(t_FFunction const &_fFunction);
		TCStreamingFunction(t_FFunction &&_fFunction);
		
		NConcurrency::TCContinuation<NContainer::TCVector<uint8, NMem::CAllocator_HeapSecure>> f_Call(CDistributedActorReadStream &_Stream) override;
		bool f_IsEmpty() const override;

		t_FFunction m_fFunction;
	};
}
