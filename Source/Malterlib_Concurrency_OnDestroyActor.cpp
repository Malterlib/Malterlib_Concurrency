// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/Actor/OnDestroy>

namespace NMib
{
	namespace NConcurrency
	{

		COnDestroyActor::COnDestroyActor()
		{
		}

		COnDestroyActor::~COnDestroyActor()
		{
			m_OnDestroyCallbacks.f_Clear();
		}

		void COnDestroyActor::fp_RemoveCallback(CCallbackHandle *_pHandle)
		{
			m_OnDestroyCallbacks.f_Remove(*_pHandle);
		}

		void COnDestroyActor::CCallbackReference::fp_RemoveCallback()
		{
			if (m_pHandle)
			{
				auto Actor = m_Actor;
				DMibCheck(Actor);

				Actor(&COnDestroyActor::fp_RemoveCallback, fg_Move(m_pHandle))
					> Actor / [&](TCAsyncResult<void> &&_Result)
					{
						_Result.f_Get();
					}
				;
				m_pHandle = nullptr;
			}
		}

		COnDestroyActor::CCallbackReference &COnDestroyActor::CCallbackReference::operator =(COnDestroyActor::CCallbackReference &&_Other)
		{ 
			fp_RemoveCallback();
			m_pHandle = fg_Move(_Other.m_pHandle);
			m_Actor = fg_Move(_Other.m_Actor);

			DMibCheck(!m_pHandle || m_Actor);

			_Other.m_pHandle = nullptr;
			return *this;
		}


		COnDestroyActor::CCallbackReference::~CCallbackReference()
		{
			fp_RemoveCallback();
		}
		
		COnDestroyActor::CCallback::~CCallback()
		{
		}

		COnDestroyActor::CCallbackReference::CCallbackReference()
			: m_pHandle(nullptr)
		{
		}

		COnDestroyActor::CCallbackReference::CCallbackReference(CCallbackReference &&_Other)
			: m_pHandle(_Other.m_pHandle)
			, m_Actor(fg_Move(_Other.m_Actor))
		{ 
			DMibRequire(!m_pHandle || m_Actor);
			_Other.m_pHandle = nullptr;
		}


	}
}

