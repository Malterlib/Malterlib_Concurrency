// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	template <typename t_CType, typename t_CLock>
	class TCLockActor : public CActor
	{
		t_CType &m_Object;
		t_CLock &m_Lock;
		aint m_LockSequence;
		align_cacheline NAtomic::TCAtomic<smint> m_Locked;
		NThread::CEventAutoReset m_Event;

		void fp_Unlock(aint _LockSequence);
		void f_Unlock(aint _LockSequence);
		TCContinuation<void> fp_Destroy() override;
	public:
		typedef CSeparateThreadActorHolder CActorHolder;

		TCLockActor(t_CType &_Object, t_CLock &_Lock);
		~TCLockActor();

		class CLockReference
		{
			friend class TCLockActor;
			TCLockActor *m_pActor;
			aint m_LockSequence;
			CLockReference(TCLockActor *_pActor, aint _LockSequence);
			CLockReference(CLockReference const &_Other);
			CLockReference &operator =(CLockReference const &_Other);

		public:
			CLockReference(CLockReference &&_Other);
			CLockReference &operator =(CLockReference &&_Other);
			~CLockReference();
			t_CType &operator *();
		};

		CLockReference f_Lock();
	};
}

#include "Malterlib_Concurrency_LockActor.hpp"
