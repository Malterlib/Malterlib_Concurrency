// Copyright © 2021 Favro Holding AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>

namespace NMib::NConcurrency
{
	class COSMainRunLoopActorHolder : public CDefaultActorHolder
	{
	public:
		COSMainRunLoopActorHolder
			(
				CConcurrencyManager *_pConcurrencyManager
				, bool _bImmediateDelete
				, EPriority _Priority
				, NStorage::TCSharedPointer<ICDistributedActorData> &&_pDistributedActorData
			)
		;
		~COSMainRunLoopActorHolder();

	protected:
		void fp_StartQueueProcessing() override;
		void fp_DestroyThreaded() override;
		void fp_QueueProcessDestroy(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess(FActorQueueDispatch &&_Functor) override;
		void fp_QueueProcess();
	};
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
