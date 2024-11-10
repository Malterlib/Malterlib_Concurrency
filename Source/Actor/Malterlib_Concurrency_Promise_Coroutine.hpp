// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

namespace NMib::NConcurrency
{
	inline_always CFutureCoroutineContext::CFutureCoroutineContext(ECoroutineFlag _Flags) noexcept
		: m_Flags(_Flags)
	{
#if DMibConfig_Tests_Enable
		auto &ThreadLocal = fg_SystemThreadLocal();
		m_Flags |= ThreadLocal.m_ExtraCoroutineFlags;
#endif
	}

	inline_always CFutureCoroutineContext::~CFutureCoroutineContext() noexcept
	{
		auto &SystemThreadLocal = fg_SystemThreadLocal();

		if (m_bRuntimeStateConstructed) [[unlikely]] // Only happens on exceptions, otherwise final_suspend should take care of this
		{
			if (m_bLinkConstructed)
			{
				DMibFastCheck(!m_bPreviousCoroutineHandlerConstructed);
				DMibFastCheck(m_State.m_Link.f_IsInList());
				m_State.m_Link.f_UnsafeUnlink();
#if DMibEnableSafeCheck > 0
				m_bLinkConstructed = false;
#endif
			}
			else if (m_bPreviousCoroutineHandlerConstructed)
			{
				DMibFastCheck(m_State.m_pPreviousCoroutineHandler != &m_CoroutineHandlers);

				DMibFastCheck(SystemThreadLocal.m_pCurrentCoroutineHandler == &m_CoroutineHandlers);
				SystemThreadLocal.m_pCurrentCoroutineHandler = m_State.m_pPreviousCoroutineHandler;
			}

			auto &ConcurrencyThreadLocal = *SystemThreadLocal.m_PromiseThreadLocal.m_pConcurrencyThreadLocal;
			if (!m_State.m_AsyncDestructors.f_IsEmpty())
			{
				if (ConcurrencyThreadLocal.m_bCaptureAsyncDestructors)
					ConcurrencyThreadLocal.m_AsyncDestructors = fg_Move(m_State.m_AsyncDestructors);
			}

			DMibFastCheck(m_State.m_AsyncDestructors.f_IsEmpty()); // Should have been handled

			m_State.~CRuntimeState();
		}

		DMibFastCheck(!m_bLinkConstructed);

		m_RestoreScopes.f_Clear();

#if DMibEnableSafeCheck > 0
		m_pSuspendDebugException = nullptr;
#endif

		if (m_pPromiseData->m_BeforeSuspend.m_bAllocatedInCoroutineContext) [[likely]]
			SystemThreadLocal.m_PromiseThreadLocal.f_PushAllocation(m_pPromiseData);
	}
}

namespace NMib::NConcurrency::NPrivate
{
#ifdef DCompiler_MSVC_Workaround
	template <typename tf_CPromise>
	inline_always void fg_DestroyCoroutine(TCCoroutineHandle<tf_CPromise> &_Handle)
	{
		// Workaround for https://developercommunity.visualstudio.com/t/C20-coroutines-checks-if-coroutine-has/10790720

		struct CCoroutineFrame_Cpp20
		{
			void (*m_fResume)(CCoroutineFrame_Cpp20 *);
			union
			{
				void (*m_fDestroy)(CCoroutineFrame_Cpp20 *);
				mint m_DestroyBits;
			};
		};

		struct CCoroutineFrame_Cpp17
		{
			void (*m_fResume)(CCoroutineFrame_Cpp17 *);
			uint16 m_Index;
			uint16 m_Flags;
		};

		CCoroutineFrame_Cpp20 *pCoroutineFrame = (CCoroutineFrame_Cpp20 *)_Handle.address();
		_Handle = nullptr;

		auto fDestroy = pCoroutineFrame->m_fDestroy;

		((CCoroutineFrame_Cpp17 *)pCoroutineFrame)->m_Flags = 1; // Set to allocated

		fDestroy(pCoroutineFrame);
	}
#else
	template <typename tf_CPromise>
	inline_always void fg_DestroyCoroutine(TCCoroutineHandle<tf_CPromise> &_Handle)
	{
		_Handle.destroy();
	}
#endif

	template <typename t_CReturnType>
	TCFutureCoroutineContextShared<t_CReturnType>::~TCFutureCoroutineContextShared() noexcept
	{
		if (!m_pPromiseData)
			return;

		auto &PromiseData = *static_cast<TCPromiseData<t_CReturnType> *>(m_pPromiseData);
		if (fp_DestroyShared(&PromiseData))
		{
			if (PromiseData.m_BeforeSuspend.m_bNeedAtomicRead)
				PromiseData.f_OnResult();
			else
				PromiseData.f_OnResultRelaxed();
		}
	}

	template <typename t_CReturnType>
	void TCFutureCoroutineContext<t_CReturnType>::return_value(t_CReturnType &&_Value)
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<t_CReturnType> *>(this->m_pPromiseData);
		pPromiseData->f_SetResultNoReport(fg_Move(_Value));
#endif
	}

	template <typename t_CReturnType>
	template <typename tf_CReturnType>
	void TCFutureCoroutineContext<t_CReturnType>::return_value(tf_CReturnType &&_Value)
	{
#ifndef DMibClangAnalyzerWorkaround
		auto *pPromiseData = static_cast<TCPromiseData<t_CReturnType> *>(this->m_pPromiseData);

		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<t_CReturnType>>::mc_Value)
			pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NException::TCIsException<CReturnNoReference>::mc_Value)
		{
			static_assert
				(
					(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
					&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
					, "Only safe to return newly created exceptions, otherwise use exception pointers"
				)
			;
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value)
			pPromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		else
			pPromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
#endif
	}

	template <typename tf_CReturnType>
	void TCFutureCoroutineContext<void>::return_value(tf_CReturnType &&_Value)
	{
#ifndef DMibClangAnalyzerWorkaround
		using CReturnNoReference = typename NTraits::TCRemoveReferenceAndQualifiers<tf_CReturnType>::CType;
		static_assert
			(
				NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value
				|| NTraits::TCIsSame<CReturnNoReference, TCPromise<void>>::mc_Value
				|| NException::TCIsException<CReturnNoReference>::mc_Value
				|| NTraits::TCIsSame<CReturnNoReference, NException::CExceptionPointer>::mc_Value
				, "You can only return exceptions, async results or promises"
			)
		;

		auto *PromiseData = static_cast<TCPromiseData<void> *>(m_pPromiseData);

		if constexpr (NTraits::TCIsSame<CReturnNoReference, TCAsyncResult<void>>::mc_Value)
			PromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else if constexpr (NTraits::TCIsSame<CReturnNoReference, TCPromise<void>>::mc_Value)
			PromiseData->f_SetResultNoReport(fg_Forward<tf_CReturnType>(_Value));
		else
		{
			if constexpr (NException::TCIsException<CReturnNoReference>::mc_Value)
			{
				static_assert
					(
						(NTraits::TCIsRValueReference<tf_CReturnType>::mc_Value || !NTraits::TCIsReference<tf_CReturnType>::mc_Value)
						&& !NTraits::TCIsConst<typename NTraits::TCRemoveReference<tf_CReturnType>::CType>::mc_Value
						, "Only safe to newly created exceptions, otherwise use exception pointers"
					)
				;
				PromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
			}
			else
				PromiseData->f_SetExceptionNoReport(fg_Forward<tf_CReturnType>(_Value));
		}
#endif
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> TCFutureCoroutineContextShared<t_CReturnType>::fp_GetReturnObject()
	{
		auto &ThreadLocal = fg_SystemThreadLocal();
		auto &PromiseThreadLocal = ThreadLocal.m_PromiseThreadLocal;

		void *pPromiseMemory;
		if (PromiseThreadLocal.m_pUsePromise || !PromiseThreadLocal.m_pCoroutinePromiseAllocation)
			pPromiseMemory = nullptr;
		else
			pPromiseMemory = PromiseThreadLocal.f_PopAllocation();

		DMibFastCheck(pPromiseMemory || !PromiseThreadLocal.m_bSupendOnInitialSuspend);

		NStorage::TCSharedPointer<TCPromiseData<t_CReturnType>> pPromiseDataShared = fg_ConsumePromise
			<
				t_CReturnType
				, EConsumePromiseFlag_UsePromise | EConsumePromiseFlag_HaveAllocation | EConsumePromiseFlag_FromCoroutine
			>
			(
				PromiseThreadLocal
				, pPromiseMemory
				, CPromiseDataBaseRefCount::mc_FutureBit - (PromiseThreadLocal.m_bSupendOnInitialSuspend ? 0 : 1)
#if DMibEnableSafeCheck > 0
				, this
				, EConsumePromiseDebugFlag_None
#endif
			)
		;

		fp_GetReturnObjectShared
			(
				pPromiseDataShared.f_Get()
				, PromiseThreadLocal
#if DMibEnableSafeCheck > 0
				, false
#endif
			)
		;

		return pPromiseDataShared;
	}

	template <typename t_CReturnType>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> TCFutureCoroutineContextShared<t_CReturnType>::fp_GetReturnObjectGenerator(CPromiseThreadLocal &_ThreadLocal)
	{
		NStorage::TCSharedPointer<TCPromiseData<t_CReturnType>> pPromiseDataShared = fg_Construct(_ThreadLocal, 0);

		fp_GetReturnObjectShared
			(
				pPromiseDataShared.f_Get()
				, _ThreadLocal
#if DMibEnableSafeCheck > 0
				, true
#endif
			)
		;

		return pPromiseDataShared;
	}

	template <typename t_CReturnType>
	mark_no_coroutine_debug TCFuture<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::get_return_object()
	{
		return TCFuture<t_CReturnType>(fp_GetReturnObject());
	}

	template <typename t_CReturnType>
	auto TCFutureCoroutineContextShared<t_CReturnType>::f_ConstructRunQueueEntry() -> CConcurrentRunQueueEntryImpl *
	{
		static_assert(sizeof(CConcurrentRunQueueEntryImpl) == sizeof(m_RunQueueImplStorage));
		DMibFastCheck(!m_bRuntimeStateConstructed);

		return new (m_RunQueueImplStorage) CConcurrentRunQueueEntryImpl();
	}

	template <typename t_CReturnType>
	auto TCFutureCoroutineContextShared<t_CReturnType>::fs_KeepaliveSetExceptionFunctor(CFutureCoroutineContext &_Context, TCActor<CActor> &&_Actor) -> FDeliverExceptionResult
	{
		auto *pThis = static_cast<TCFutureCoroutineContextShared<t_CReturnType> *>(&_Context);

		return [KeepAlive = pThis->f_KeepAlive(fg_Move(_Actor))](NException::CExceptionPointer &&_pException)
			{
				KeepAlive.mp_pPromiseData->f_SetExceptionNoReport(fg_Move(_pException));
			}
		;
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::TCFutureCoroutineKeepAlive(TCActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: TCFutureCoroutineKeepAliveImplicit<t_CReturnType>(fg_Move(_pPromiseData))
		, mp_pActor(_Actor.f_Unsafe_AccessInternal().f_Detach())
	{
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::TCFutureCoroutineKeepAlive(TCWeakActor<CActor> &&_Actor, NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: TCFutureCoroutineKeepAliveImplicit<t_CReturnType>(fg_Move(_pPromiseData))
		, mp_pActor(_Actor.f_Unsafe_AccessInternal().f_Unsafe_Detach(), 1)
	{
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::TCFutureCoroutineKeepAlive(TCFutureCoroutineKeepAlive &&_Other)
		: TCFutureCoroutineKeepAliveImplicit<t_CReturnType>(fg_Move(_Other))
		, mp_pActor(_Other.mp_pActor, _Other.mp_pActor.f_GetBits())
	{
		_Other.mp_pActor = nullptr;
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType>::~TCFutureCoroutineKeepAlive()
	{
		auto pActor = mp_pActor.f_Get();
		if (pActor)
		{
			if (mp_pActor.f_GetBits()) [[unlikely]]
			{
				TCWeakActor<CActor> ToDestroy(TCActorHolderWeakPointer<TCActorInternal<CActor>>(fg_Attach(pActor)));
			}
			else
			{
				TCActor<CActor> ToDestroy(TCActorHolderSharedPointer<TCActorInternal<CActor>>(fg_Attach(pActor)));
			}
		}
	}

	template <typename t_CReturnType>
	TCActor<CActor> TCFutureCoroutineKeepAlive<t_CReturnType>::f_MoveActor()
	{
		auto pActor = mp_pActor.f_Get();
		if (pActor)
		{
			mp_pActor = nullptr;
			if (mp_pActor.f_GetBits()) [[unlikely]]
			{
				TCWeakActor<CActor> ToMove(TCActorHolderWeakPointer<TCActorInternal<CActor>>(fg_Attach(pActor)));

				return ToMove.f_Lock();
			}
			else
				return TCActor<CActor>(TCActorHolderSharedPointer<TCActorInternal<CActor>>(fg_Attach(pActor)));
		}
		else
			return {};
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType> &&TCFutureCoroutineKeepAlive<t_CReturnType>::f_MoveImplicit()
	{
		return fg_Move(*this);
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::TCFutureCoroutineKeepAliveImplicit(NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnType>> &&_pPromiseData)
		: mp_pPromiseData(fg_Move(_pPromiseData))
	{
		DMibFastCheck(mp_pPromiseData);
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::TCFutureCoroutineKeepAliveImplicit(TCFutureCoroutineKeepAliveImplicit &&_Other)
		: mp_pPromiseData(fg_Move(_Other.mp_pPromiseData))
	{
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::~TCFutureCoroutineKeepAliveImplicit()
	{
		auto *pPromiseData = mp_pPromiseData.f_Detach();
		if (!pPromiseData)
			return;

		auto &PromiseData = *pPromiseData;

		if (PromiseData.m_RefCount.f_IsLastPromiseReference() && PromiseData.m_AfterSuspend.m_bPendingResult && !PromiseData.m_AfterSuspend.m_bCoroutineValid)
		{
			PromiseData.m_AfterSuspend.m_bPendingResult = false;
			PromiseData.f_OnResult();
		}

		if (PromiseData.m_RefCount.f_PromiseDecrease())
			fg_DeleteObject(NMemory::CDefaultAllocator(), pPromiseData);
	}

	template <typename t_CReturnType>
	bool TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::f_HasValidCoroutine() const
	{
		DMibFastCheck(mp_pPromiseData);
		return mp_pPromiseData->m_AfterSuspend.m_bCoroutineValid;
	}

	template <typename t_CReturnType>
	auto TCFutureCoroutineKeepAliveImplicit<t_CReturnType>::f_Copy() const -> TCFutureCoroutineKeepAliveImplicit
	{
		return fg_TempCopy(mp_pPromiseData);
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAlive<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::f_KeepAlive(TCActor<> &&_Actor)
	{
		DMibFastCheck(_Actor);
		auto pPromiseData = static_cast<TCPromiseData<t_CReturnType> *>(m_pPromiseData);
		pPromiseData->m_BeforeSuspend.f_NeedAtomicRead();

		if (this->m_Flags & ECoroutineFlag_BreakSelfReference)
			return TCFutureCoroutineKeepAlive<t_CReturnType>(_Actor.f_Weak(), fg_Explicit(pPromiseData));
		else
			return TCFutureCoroutineKeepAlive<t_CReturnType>(fg_Move(_Actor), fg_Explicit(pPromiseData));
	}

	template <typename t_CReturnType>
	TCFutureCoroutineKeepAliveImplicit<t_CReturnType> TCFutureCoroutineContextShared<t_CReturnType>::f_KeepAliveImplicit()
	{
		auto pPromiseData = static_cast<TCPromiseData<t_CReturnType> *>(m_pPromiseData);
		pPromiseData->m_BeforeSuspend.f_NeedAtomicRead();

		return TCFutureCoroutineKeepAliveImplicit<t_CReturnType>(fg_Explicit(pPromiseData));
	}
}

namespace NMib::NConcurrency
{
	struct CFutureCoroutineContext::CFinalAwaiter
	{
		CFinalAwaiter(TCFuture<void> &&_Future)
			: mp_Future(fg_Move(_Future))
		{
		}

		bool await_ready() noexcept
		{
#ifndef DCompiler_MSVC_Workaround
			if (!mp_Future.f_IsValid())
				return true;
#endif
			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle) noexcept
		{
			auto &CoroutineContext = _Handle.promise();

#ifdef DCompiler_MSVC_Workaround
			if (!mp_Future.f_IsValid())
			{
#if DMibEnableSafeCheck > 0
				auto &ThreadLocal = fg_ConcurrencyThreadLocal().m_SystemThreadLocal.m_PromiseThreadLocal;
				auto pPreAllocation = ThreadLocal.m_pCoroutinePromiseAllocation;
#endif
				NPrivate::fg_DestroyCoroutine(_Handle);
				DMibFastCheck(ThreadLocal.m_pCoroutinePromiseAllocation == pPreAllocation);
				return true;
			}
#endif
			DMibFastCheck(fg_CurrentActor());

			auto Future = fg_Move(mp_Future);
			if
				(
					Future.f_OnResultSetOrAvailable
					(
						[Actor = fg_CurrentActor(), KeepAlive = CoroutineContext.f_KeepAliveImplicit()](TCAsyncResult<void> &&_Result) mutable
						{
							Actor.f_GetRealActor()->f_QueueProcess
								(
									[Actor, KeepAlive = fg_Move(KeepAlive), Result = fg_Move(_Result)](CConcurrencyThreadLocal &_ThreadLocal) mutable
									{
										auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();

										if (!Result)
											KeepAlive.f_PromiseData().f_SetExceptionNoReportAppendable(fg_Move(Result).f_GetException());

										NPrivate::fg_DestroyCoroutine(Handle);
									}
								)
							;
						}
					)
				)
			{
				auto Result = Future.f_MoveResult();
				if (!Result)
					CoroutineContext.f_PromiseData()->f_SetExceptionNoReportAppendable(fg_Move(Result).f_GetException());

#ifdef DCompiler_MSVC_Workaround
				NPrivate::fg_DestroyCoroutine(_Handle);
				return true;
#else
				return false;
#endif
			}

			return true;
		}

		void await_resume() noexcept
		{
		}

	private:
		TCFuture<void> mp_Future;
	};

	struct CFutureCoroutineContext::CInitialSuspend
	{
		inline_always bool await_ready() const noexcept
		{
			return m_bInitialReady;
		}

		inline_always void await_suspend(TCCoroutineHandle<>) const noexcept
		{
		}

		inline_always void await_resume() const noexcept
		{
		}

		bool m_bInitialReady = true;
	};


	template <typename t_CReturnType, bool t_bUnwrap, typename t_FExceptionTransform>
	struct TCFutureAwaiter
	{
		TCFutureAwaiter(TCFuture<t_CReturnType> &&_Future, t_FExceptionTransform &&_fExceptionTransform)
			: mp_Future(fg_Move(_Future))
			, mp_fExceptionTransform(fg_Move(_fExceptionTransform))
		{
		}

		~TCFutureAwaiter()
		{
			if (mp_bForceDelete)
				mp_Future.f_ForceDelete();
		}

		bool await_ready() noexcept
		{
			auto &PromiseData = *mp_Future.f_Unsafe_PromiseData();

			if (!PromiseData.f_CanUseRelaxedAtomicsForRead())
				return false;

			if (mp_Future.f_ObserveIfAvailableRelaxed())
			{
				mp_bForceDelete = true;

				if constexpr (t_bUnwrap)
				{
					if (PromiseData.m_Result)
						return true;
				}
				else
					return true;
			}

			return false;
		}

		template <typename tf_CCoroutineContext>
		bool await_suspend(TCCoroutineHandle<tf_CCoroutineContext> &&_Handle) noexcept(NTraits::TCIsSame<t_FExceptionTransform, CVoidTag>::mc_Value)
		{
			auto &ThreadLocal = fg_ConcurrencyThreadLocal();
			DMibFastCheck(fg_CurrentActor(ThreadLocal));

			auto &CoroutineContext = _Handle.promise();

			auto &PromiseData = *mp_Future.f_Unsafe_PromiseData();
			if constexpr (t_bUnwrap)
			{
				if (mp_bForceDelete)
				{
					DMibFastCheck(PromiseData.m_Result.f_IsSet() && !PromiseData.m_Result);
					CoroutineContext.f_HandleAwaitedException
						(
							ThreadLocal
							, &tf_CCoroutineContext::fs_KeepaliveSetExceptionFunctor
							, fg_TransformException(fg_Move(PromiseData.m_Result).f_GetException(), mp_fExceptionTransform)
						)
					;
					return true;
				}
			}

			if
				(
					mp_Future.f_OnResultSetOrAvailable
					(
						[
							this
							, KeepAlive = CoroutineContext.f_KeepAlive(fg_ThisActor(ThreadLocal.m_pCurrentlyProcessingActorHolder))
						]
						(TCAsyncResult<t_CReturnType> &&_Result) mutable
						{
							auto Actor = KeepAlive.f_MoveActor();
							if (!Actor)
								return;

							auto pActor = Actor.f_Get();
							pActor->f_QueueProcess
								(
									[
										this
										, KeepAlive = KeepAlive.f_MoveImplicit()
#if DMibEnableSafeCheck > 0
										, pActor
#endif
									]
									(CConcurrencyThreadLocal &_ThreadLocal) mutable
									{
										DMibFastCheck(_ThreadLocal.m_pCurrentlyProcessingActorHolder == pActor);

										if (!KeepAlive.f_HasValidCoroutine())
											return; // Can happen when f_Suspend throws or co-routines are aborted in fp_DestroyInternal

										auto &PromiseData = *mp_Future.f_Unsafe_PromiseData();
										auto Handle = KeepAlive.template f_Coroutine<CFutureCoroutineContext>();

										auto &CoroutineContext = Handle.promise();
										if constexpr (t_bUnwrap)
										{
											if (!PromiseData.m_Result)
											{
												CoroutineContext.f_HandleAwaitedException
													(
														_ThreadLocal
														, &tf_CCoroutineContext::fs_KeepaliveSetExceptionFunctor
														, fg_TransformException(fg_Move(PromiseData.m_Result).f_GetException(), mp_fExceptionTransform)
													)
												;
												return;
											}
										}

										{
											bool bAborted = false;
											auto RestoreStates = CoroutineContext.f_Resume
												(
													_ThreadLocal.m_SystemThreadLocal
													, &tf_CCoroutineContext::fs_KeepaliveSetExceptionFunctor
													, bAborted
												)
											;
											if (!bAborted)
												Handle.resume();
										}
									}
								)
							;
						}
					)
				)
			{
				if constexpr (t_bUnwrap)
				{
					if (!PromiseData.m_Result)
					{
						CoroutineContext.f_HandleAwaitedException
							(
								ThreadLocal
								, &tf_CCoroutineContext::fs_KeepaliveSetExceptionFunctor
								, fg_TransformException(fg_Move(PromiseData.m_Result).f_GetException(), mp_fExceptionTransform)
							)
						;
						return true;
					}
				}

				return false;
			}

			CoroutineContext.f_Suspend(ThreadLocal.m_SystemThreadLocal, true);
			return true;
		}

		auto await_resume() noexcept
		{
			auto &PromiseData = *mp_Future.f_Unsafe_PromiseData();

			if constexpr (t_bUnwrap)
			{
				DMibFastCheck(PromiseData.m_Result);
				return fg_Move(*PromiseData.m_Result);
			}
			else
			{
				if constexpr (NTraits::TCIsSame<t_FExceptionTransform, CVoidTag>::mc_Value)
					return fg_Move(PromiseData.m_Result);
				else
				{
					if (PromiseData.m_Result)
						return fg_Move(PromiseData.m_Result);
					else
					{
						TCAsyncResult<t_CReturnType> Result;
						Result.f_SetException(mp_fExceptionTransform(fg_Move(PromiseData.m_Result.f_GetException())));
						return Result;
					}
				}
			}
		}

	private:
		TCFuture<t_CReturnType> mp_Future;
		DMibNoUniqueAddress t_FExceptionTransform mp_fExceptionTransform;
		bool mp_bForceDelete = false;
	};

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &TCFuture<t_CReturnValue>::f_Unsafe_PromiseData() const
	{
		return mp_pData;
	}

	template <typename t_CReturnValue>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> &&TCFuture<t_CReturnValue>::f_Unsafe_MovePromiseData() &&
	{
		return fg_Move(mp_pData);
	}

	template <typename t_CReturnValue>
	NStorage::TCSharedPointer<NPrivate::TCPromiseData<t_CReturnValue>> const &TCPromise<t_CReturnValue>::f_Unsafe_PromiseData() const
	{
		return mp_pData;
	}

#if DMibEnableSafeCheck > 0
	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_Debug_HasData(void const *_pData) const
	{
#if DMibConfig_Concurrency_DebugFutures
		return _pData == (void const *)static_cast<NPrivate::CPromiseDataBase const *>(mp_pData.f_Get());
#else
		return _pData == (void const *)mp_pData.f_Get();
#endif
	}

	template <typename t_CReturnValue>
	bool TCFuture<t_CReturnValue>::f_Debug_HadCoroutine(void const *_pData) const
	{
		return mp_pData->m_pHadCoroutine == _pData;
	}

	template <typename t_CReturnValue>
	bool TCPromise<t_CReturnValue>::f_Debug_HadCoroutine(void const *_pData) const
	{
		return mp_pData->m_pHadCoroutine == _pData;
	}
#endif

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, true>(fg_Move(*this), {});
	}

	template <typename t_CReturnValue>
	auto TCFuture<t_CReturnValue>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		return TCFutureAwaiter<t_CReturnValue, false>(fg_Move(*m_pWrapped), {});
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::TCFutureWithExceptionTransformer(t_CFutureLike &&_Future, FExceptionTransformer &&_fTransfomer)
		: CActorWithErrorBase(fg_Move(_fTransfomer))
		, m_Future(fg_Move(_Future))
	{
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::f_Wrap() && -> CNoUnwrapAsyncResult
	{
		return {this};
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	TCFuture<t_CReturnValue> TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::f_Dispatch() &&
	{
		TCPromise<t_CReturnValue> Promise{CPromiseConstructNoConsume()};
		fg_Move(m_Future).f_OnResultSet
			(
				[Promise, fTransformer = fg_Move(*this).f_GetTransformer()](TCAsyncResult<t_CReturnValue> &&_Result)
				{
					if (!_Result)
						return Promise.f_SetException(fTransformer(fg_Move(_Result).f_GetException()));

					if constexpr (NTraits::TCIsVoid<t_CReturnValue>::mc_Value)
						Promise.f_SetResult();
					else
						Promise.f_SetResult(fg_Move(*_Result));
				}
			)
		;
		return Promise.f_MoveFuture();
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	template <typename tf_CType>
	inline_always auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::operator + (TCFuture<tf_CType> &&_Other) &&
	{
		return fg_Move(*this).f_Dispatch() + fg_Move(_Other);
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	template <typename tf_CReturn, typename tf_CFutureLike>
	inline_always auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::operator + (TCFutureWithExceptionTransformer<tf_CReturn, tf_CFutureLike> &&_Other) &&
	{
		return fg_Move(*this).f_Dispatch() + fg_Move(_Other).f_Dispatch();
	}


	template <typename t_CReturnValue, typename t_CFutureLike>
	auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::operator co_await() &&
	{
		if constexpr (NTraits::TCIsSame<t_CFutureLike, TCFuture<t_CReturnValue>>::mc_Value)
		{
			return TCFutureAwaiter<t_CReturnValue, true, FExceptionTransformer>
				(
					fg_Move(m_Future)
					, fg_Move(*this).f_GetTransformer()
				)
			;
		}
		else
		{
			return TCBoundActorCallAwaiter<t_CReturnValue, t_CFutureLike, true, FExceptionTransformer>
				(
					fg_Move(m_Future)
					, fg_Move(*this).f_GetTransformer()
				)
			;
		}
	}

	template <typename t_CReturnValue, typename t_CFutureLike>
	auto TCFutureWithExceptionTransformer<t_CReturnValue, t_CFutureLike>::CNoUnwrapAsyncResult::operator co_await() &&
	{
		if constexpr (NTraits::TCIsSame<t_CFutureLike, TCFuture<t_CReturnValue>>::mc_Value)
		{
			return TCFutureAwaiter<t_CReturnValue, false, FExceptionTransformer>
				(
					fg_Move(m_pWrapped->m_Future)
					, fg_Move(*m_pWrapped).f_GetTransformer()
				)
			;
		}
		else
		{
			return TCBoundActorCallAwaiter<t_CReturnValue, t_CFutureLike, false, FExceptionTransformer>
				(
					fg_Move(m_pWrapped->m_Future)
					, fg_Move(*m_pWrapped).f_GetTransformer()
				)
			;
		}
	}
}

namespace NMib::NConcurrency::NPrivate
{
	template <typename tfp_CThis, bool t_bIsComplete = NTraits::TCIsComplete<tfp_CThis>::mc_Value>
	struct TCCoroutineContextFlagsFromParams_This
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag_UnsafeThisPointer; // Assume lambda
	};

	template <bool t_bIsComplete>
	struct TCCoroutineContextFlagsFromParams_This<void, t_bIsComplete>
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag_None; // Free functions
	};

	template <typename tfp_CThis>
	struct TCCoroutineContextFlagsFromParams_This<tfp_CThis, true>
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value =
			NTraits::TCIsReference<tfp_CThis>::mc_Value
			?
			(
				(
					NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActor>::mc_Value
					|| NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CActorInternal>::mc_Value
					|| NTraits::TCIsBaseOf<typename NTraits::TCRemoveReference<tfp_CThis>::CType, CAllowUnsafeThis>::mc_Value
				)
				? NMib::NConcurrency::ECoroutineFlag_None
				: NMib::NConcurrency::ECoroutineFlag_UnsafeThisPointer
			)
			: NMib::NConcurrency::ECoroutineFlag_None
		;
	};

	template <typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag_None;
	};

	template <typename tp_CFirstParam, typename ...tp_CParams>
	struct TCCoroutineContextFlagsFromParams<tp_CFirstParam, tp_CParams...>
	{
		static constexpr NMib::NConcurrency::ECoroutineFlag mc_Value = NMib::NConcurrency::ECoroutineFlag
			(
				(
					(... || NMib::NTraits::TCIsReference<tp_CParams>::mc_Value)
					? NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
					: NMib::NConcurrency::ECoroutineFlag_None
				)
				| TCCoroutineContextFlagsFromParams_This<tp_CFirstParam>::mc_Value
			)
		;
	};
}

namespace std
{
#ifdef _LIBCPP_HAS_EXTENDED_COROUTINE_TRAITS
	template <typename t_CReturnType, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
			)
			|| !!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
	struct extended_coroutine_traits<NMib::NConcurrency::TCFuture<t_CReturnType>, tp_CParams...>
	{
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContext<t_CReturnType>;
#endif
	};

	template <typename t_CReturnType, NMib::NConcurrency::ECoroutineFlag t_Flags, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags) & NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
			)
			|| !!
			(
				(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags) & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
	struct extended_coroutine_traits<NMib::NConcurrency::TCFutureWithFlags<t_CReturnType, t_Flags>, tp_CParams...>
	{
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::ECoroutineFlag(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags)
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags<t_CReturnType, t_Flags>;
#endif
	};
#endif

	template <typename t_CReturnType, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
			)
			|| !!
			(
				NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
	struct coroutine_traits<NMib::NConcurrency::TCFuture<t_CReturnType>, tp_CParams...>
	{
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContext<t_CReturnType>;
#endif
	};

	template <typename t_CReturnType, NMib::NConcurrency::ECoroutineFlag t_Flags, typename ...tp_CParams>
		// Reference parameters in co-routines are not safe. If you want to take responsibility, make the param a pointer instead or use TCUnsafeFuture
		requires
		(
			!
			(
				(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags) & NMib::NConcurrency::ECoroutineFlag_UnsafeReferenceParameters
			)
			|| !!
			(
				(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags) & NMib::NConcurrency::ECoroutineFlag_AllowReferences
			)
		)
	struct coroutine_traits<NMib::NConcurrency::TCFutureWithFlags<t_CReturnType, t_Flags>, tp_CParams...>
	{
#if DMibEnableSafeCheck > 0
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags
			<
				t_CReturnType
				, NMib::NConcurrency::ECoroutineFlag(NMib::NConcurrency::NPrivate::TCCoroutineContextFlagsFromParams<tp_CParams...>::mc_Value | t_Flags)
			>
		;
#else
		using promise_type = NMib::NConcurrency::NPrivate::TCFutureCoroutineContextWithFlags<t_CReturnType, t_Flags>;
#endif
	};
}
