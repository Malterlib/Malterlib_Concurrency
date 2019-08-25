// Copyright © 2019 Nonna Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	extern template NMib::NConcurrency::TCActorCall<NMib::NConcurrency::TCActor<NMib::NConcurrency::CDirectCallActor>, NMib::NConcurrency::TCFuture<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> > (NMib::NConcurrency::CActor::*)(NMib::NFunction::TCFunctionMovable<NMib::NConcurrency::TCFuture<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> > ()> &&), NStorage::TCTuple<NMib::NFunction::TCFunctionMovable<NMib::NConcurrency::TCFuture<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> > ()> >, NMib::NMeta::TCTypeList<NMib::NFunction::TCFunctionMovable<NMib::NConcurrency::TCFuture<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> > ()> >, true>::~TCActorCall();
	extern template NMib::NConcurrency::TCActor<NMib::NConcurrency::CActor>::~TCActor();
	extern template NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> >::~TCPromiseData();
	extern template NMib::NConcurrency::TCActor<NMib::NConcurrency::CActor> NMib::NConcurrency::TCWeakActor<NMib::NConcurrency::CActor>::f_Lock() const;
	extern template void NMib::NConcurrency::fg_DeleteWeakObject<NMib::NConcurrency::TCActorInternal<NMib::NConcurrency::CActor>>(CInternalActorAllocator &, NMib::NConcurrency::TCActorInternal<NMib::NConcurrency::CActor> *);
	extern template TCFuture<NContainer::TCVector<TCAsyncResult<void>>> NMib::NConcurrency::TCActorResultVector<void>::CInternal::f_GetResults();
}
namespace NMib::NStorage
{
	extern template NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> >>::~TCSharedPointer();
	extern template bool NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> >>::fp_Delete();
	extern template bool NMib::NStorage::TCSharedPointer<NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> >>::fp_Delete<false, nullptr>();

	extern template NMib::NStorage::TCSharedPointer<NMib::NConcurrency::TCActorInternal<NMib::NConcurrency::CActor>, NMib::NStorage::CSupportWeakTag, NMib::NConcurrency::CInternalActorAllocator>::~TCSharedPointer();
	extern template bool NMib::NStorage::TCSharedPointer<NMib::NConcurrency::TCActorInternal<NMib::NConcurrency::CActor>, NMib::NStorage::CSupportWeakTag, NMib::NConcurrency::CInternalActorAllocator>::fp_Delete();
	extern template bool NMib::NStorage::TCSharedPointer<NMib::NConcurrency::TCActorInternal<NMib::NConcurrency::CActor>, NMib::NStorage::CSupportWeakTag, NMib::NConcurrency::CInternalActorAllocator>::fp_Delete<true, nullptr>();
	extern template NMib::NStorage::TCSharedPointer<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >>::TCSharedPointer();
	extern template NMib::NStorage::TCSharedPointer<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >>::TCSharedPointer(TCConstruct<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, NMib::NFunction::TCFunctionMovable<void ()> > &&);
}
namespace NMib::NContainer
{
	extern template NMib::NContainer::TCVector<NException::CExceptionPointer, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault>::TCVector();
	extern template NMib::NContainer::TCVector<NException::CExceptionPointer, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault>::TCVector(NMib::NContainer::TCVector<NException::CExceptionPointer, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> &&);
}
namespace NMib
{
	extern template void NMib::fg_DeleteObject<NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> >, NMib::NMemory::CAllocator_Heap &>(NMib::NMemory::CAllocator_Heap &, NMib::NConcurrency::NPrivate::TCPromiseData<NMib::NContainer::TCVector<NMib::NConcurrency::TCAsyncResult<void>, NMib::NMemory::CAllocator_Heap, NMib::NContainer::CVectorOptionsDefault> > *, mint);
	extern template NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0> *
	NMib::TCConstruct<NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0>, NMib::NFunction::TCFunctionMovable<void ()> >::f_Create<NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0>, NMib::NMemory::CAllocator_Heap &>(NMib::NMemory::CAllocator_Heap &);
	extern template NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0> *
	NMib::TCConstruct<NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0>, NMib::NFunction::TCFunctionMovable<void ()> >::fp_Create<NMib::NStorage::NPrivate::TCSharedPointerCounter<NMib::TCOnScopeExit<NFunction::TCFunctionMovable<void ()> >, false, 0>, NMib::NMemory::CAllocator_Heap &, 0>(NMib::NMemory::CAllocator_Heap &, NMeta::TCIndices<0> const& );
}
