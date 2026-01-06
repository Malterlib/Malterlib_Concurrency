// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Lock>

namespace NMib::NConcurrency
{
	template <typename t_CType, typename t_CLock>
	void TCLockActor<t_CType, t_CLock>::fp_Unlock(aint _LockSequence)
	{
		auto &State = *mp_pState;
		if (State.m_LockSequence == _LockSequence)
			State.m_Lock.f_Unlock();
	}

	template <typename t_CType, typename t_CLock>
	void TCLockActor<t_CType, t_CLock>::CState::f_Unlock(aint _LockSequence)
	{
		[[maybe_unused]] aint Old = m_Locked.f_Exchange(0);

		m_Event.f_Signal();
		DMibRequire(Old == 1);
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CState::CState(t_CType &_Object, t_CLock &_Lock)
		: m_Object(_Object)
		, m_Lock(_Lock)
		, m_LockSequence(0)
	{
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::TCLockActor(t_CType &_Object, t_CLock &_Lock)
		: mp_pState(fg_Construct(_Object, _Lock))
	{
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::~TCLockActor()
	{
		auto &State = *mp_pState;
		DMibCheck(!State.m_Locked.f_Load())("Someone still owns this lock");
		if (State.m_Lock.f_OwnsLock())
			State.m_Lock.f_Unlock();
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::CLockReference(TCLockActor *_pActor, aint _LockSequence)
		: mp_pState(_pActor->mp_pState)
		, mp_pActor(fg_ThisActor(_pActor).f_Weak())
		, mp_LockSequence(_LockSequence)
	{
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::CLockReference(CLockReference &&_Other)
		: mp_pState(fg_Move(_Other.mp_pState))
		, mp_pActor(fg_Move(_Other.mp_pActor))
		, mp_LockSequence(_Other.mp_LockSequence)
	{
	}

	template <typename t_CType, typename t_CLock>
	typename TCLockActor<t_CType, t_CLock>::CLockReference &TCLockActor<t_CType, t_CLock>::CLockReference::operator =(CLockReference &&_Other)
	{
		mp_pActor = fg_Move(_Other.mp_pActor);
		mp_pState = fg_Move(_Other.mp_pState);
		mp_LockSequence = _Other.mp_LockSequence;
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::~CLockReference()
	{
		if (mp_pState)
		{
			mp_pState->f_Unlock(mp_LockSequence);
			if (auto Actor = mp_pActor.f_Lock())
				Actor.template f_Bind<&TCLockActor<t_CType, t_CLock>::fp_Unlock>(mp_LockSequence).f_DiscardResult();
		}
	}

	template <typename t_CType, typename t_CLock>
	t_CType &TCLockActor<t_CType, t_CLock>::CLockReference::operator *()
	{
		DMibRequire(mp_pState);
		return mp_pState->m_Object;
	}

	template <typename t_CType, typename t_CLock>
	typename TCLockActor<t_CType, t_CLock>::CLockReference TCLockActor<t_CType, t_CLock>::f_Lock()
	{
		auto &State = *mp_pState;
		while (true)
		{
			if (State.m_Lock.f_OwnsLock())
			{
				if (State.m_Locked.f_Exchange(1) == 0)
				{
					{
						DMibUnlock(State.m_Lock); // Give locks outside of the Lock actor a chance to lock the lock
					}
					CLockReference Reference(this, ++State.m_LockSequence);
					return Reference;
				}
				else
					State.m_Event.f_Wait();
			}
			else
			{
				State.m_Lock.f_Lock();
				auto Old = State.m_Locked.f_Exchange(1);
				(void)Old;
				DMibRequire(Old == 0);
				CLockReference Reference(this, ++State.m_LockSequence);
				return Reference;
			}
		}
	}

	template <typename t_CType, typename t_CLock>
	TCFuture<void> TCLockActor<t_CType, t_CLock>::fp_Destroy()
	{
		DMibCheck(!mp_pState->m_Locked.f_Load())("Someone still owns this lock");
		co_return {};
	}
}
