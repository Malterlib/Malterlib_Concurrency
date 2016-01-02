// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib
{
	namespace NConcurrency
	{

		class COnDestroyActor : public CActor
		{
			friend class CConcurrencyManager;
			template <typename t_CActor>
			friend class TCActorInternal;

			struct CCallback
			{
				virtual ~CCallback();
				virtual void f_DoCallback() pure;
			};

			template <typename t_CResultCall>
			struct TCCallback : public CCallback
			{
				t_CResultCall m_ResultCall;

				template <typename tf_CResultCall>
				TCCallback(tf_CResultCall &&_ResultCall);
				virtual void f_DoCallback() override;
			};

			struct CCallbackHandle
			{
				NPtr::TCUniquePointer<CCallback> m_pCallback;
			};

			NContainer::TCLinkedList<CCallbackHandle> m_OnDestroyCallbacks;

			void fp_RemoveCallback(CCallbackHandle *_pHandle);

		public:

			class CCallbackReference
			{
				friend class COnDestroyActor;
				CCallbackHandle *m_pHandle;
				TCActor<COnDestroyActor> m_Actor;

				CCallbackReference(NPtr::TCUniquePointer<CCallbackHandle> &&_Handle);
				CCallbackReference(CCallbackReference const &_Other);
				CCallbackReference &operator =(CCallbackReference const &_Other);
				void fp_RemoveCallback();
			public:
				CCallbackReference();
				~CCallbackReference();
				CCallbackReference(CCallbackReference &&_Other);
				CCallbackReference &operator =(CCallbackReference &&_Other);
			};

			COnDestroyActor();
			~COnDestroyActor();

			template <typename tf_CResultCall>
			CCallbackReference f_OnDestroy(tf_CResultCall const &_ActorCall);

		};
		

		template <typename tf_CActor, typename tf_FCallback, typename tf_CResultCall>
		void fg_OnActorDestroyed(TCActor<tf_CActor> const &_ActorToOnDestroy, tf_FCallback const &_Callback, tf_CResultCall &&_ResultCall);

	}
}

#include "Malterlib_Concurrency_OnDestroyActor.hpp"