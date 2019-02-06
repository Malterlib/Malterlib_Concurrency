// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/Lock>

namespace NMib::NConcurrency
{
	template <typename t_CType, typename t_CLock>
	void TCLockActor<t_CType, t_CLock>::fp_Unlock(aint _LockSequence)
	{
		if (m_LockSequence == _LockSequence)
			m_Lock.f_Unlock();
	}

	template <typename t_CType, typename t_CLock>
	void TCLockActor<t_CType, t_CLock>::f_Unlock(aint _LockSequence)
	{
		aint Old = m_Locked.f_Exchange(0);
		(void)Old;
		m_Event.f_Signal();
		DMibRequire(Old == 1);
		fg_ThisActor(this)(&TCLockActor::fp_Unlock, _LockSequence) > fg_DiscardResult();
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::TCLockActor(t_CType &_Object, t_CLock &_Lock)
		: m_Object(_Object)
		, m_Lock(_Lock)
		, m_LockSequence(0)
	{
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::~TCLockActor()
	{
		DMibCheck(!m_Locked.f_Load())("Someone still owns this lock");
		if (m_Lock.f_OwnsLock())
			m_Lock.f_Unlock();
	}

	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::CLockReference(TCLockActor *_pActor, aint _LockSequence)
		: m_pActor(_pActor)
		, m_LockSequence(_LockSequence)
	{
	}
	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::CLockReference(CLockReference &&_Other)
		: m_pActor(_Other.m_pActor)
		, m_LockSequence(_Other.m_LockSequence)
	{
		_Other.m_pActor = nullptr;
	}
	template <typename t_CType, typename t_CLock>
	typename TCLockActor<t_CType, t_CLock>::CLockReference &TCLockActor<t_CType, t_CLock>::CLockReference::operator =(CLockReference &&_Other)
	{
		m_pActor = _Other.m_pActor;
		m_LockSequence = _Other.m_LockSequence;
		_Other.m_pActor = nullptr;
	}
	template <typename t_CType, typename t_CLock>
	TCLockActor<t_CType, t_CLock>::CLockReference::~CLockReference()
	{
		if (m_pActor)
			m_pActor->f_Unlock(m_LockSequence);
	}

	template <typename t_CType, typename t_CLock>
	t_CType &TCLockActor<t_CType, t_CLock>::CLockReference::operator *()
	{
		DMibRequire(m_pActor);
		return m_pActor->m_Object;
	}

	template <typename t_CType, typename t_CLock>
	typename TCLockActor<t_CType, t_CLock>::CLockReference TCLockActor<t_CType, t_CLock>::f_Lock()
	{
		while (true)
		{
			if (m_Lock.f_OwnsLock())
			{
				if (m_Locked.f_Exchange(1) == 0)
				{
					{
						DMibUnlock(m_Lock); // Give locks outside of the Lock actor a chance to lock the lock
					}
					CLockReference Reference(this, ++m_LockSequence);
					return Reference;
				}
				else
					m_Event.f_Wait();
			}
			else
			{
				m_Lock.f_Lock();
				auto Old = m_Locked.f_Exchange(1);
				(void)Old;
				DMibRequire(Old == 0);
				CLockReference Reference(this, ++m_LockSequence);
				return Reference;
			}
		}
	}

	template <typename t_CType, typename t_CLock>
	TCFuture<void> TCLockActor<t_CType, t_CLock>::fp_Destroy()
	{
		DMibCheck(!m_Locked.f_Load())("Someone still owns this lock");
		return fg_Explicit();
	}
}
