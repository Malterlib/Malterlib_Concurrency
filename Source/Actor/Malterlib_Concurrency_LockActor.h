// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CType, typename t_CLock>
	struct TCLockActor : public CActor
	{
		struct CState
		{
			CState(t_CType &_Object, t_CLock &_Lock);
			void f_Unlock(aint _LockSequence);

			t_CType &m_Object;
			t_CLock &m_Lock;
			aint m_LockSequence;
			align_cacheline NAtomic::TCAtomic<smint> m_Locked;
			NThread::CEventAutoReset m_Event;
		};

		using CActorHolder = CSeparateThreadActorHolder;

		TCLockActor(t_CType &_Object, t_CLock &_Lock);
		~TCLockActor();

		struct CLockReference
		{
			CLockReference(CLockReference &&_Other);
			CLockReference &operator =(CLockReference &&_Other);
			~CLockReference();
			t_CType &operator *();

		private:
			friend struct TCLockActor;
			CLockReference(TCLockActor *_pActor, aint _LockSequence);
			CLockReference(CLockReference const &_Other);
			CLockReference &operator =(CLockReference const &_Other);

			NStorage::TCSharedPointer<CState> mp_pState;
			TCWeakActor<TCLockActor> mp_pActor;
			aint mp_LockSequence;
		};

		CLockReference f_Lock();
	private:

		void fp_Unlock(aint _LockSequence);
		TCFuture<void> fp_Destroy() override;

		NStorage::TCSharedPointer<CState> mp_pState;
	};
}

#include "Malterlib_Concurrency_LockActor.hpp"
