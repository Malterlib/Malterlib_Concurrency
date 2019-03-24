// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ActorSubscription>

#include "Malterlib_Concurrency_DistributedApp_TestHelpers.h"

namespace NMib::NConcurrency
{
	void CDistributedApp_LaunchHelper::CLaunchInfo::f_Abort()
	{
		m_Launch.f_Clear();
		m_InProcess.f_Clear();
		if (m_pClientInterface)
			m_pClientInterface->f_Clear();
		m_pClientInterface.f_Clear();
		if (m_pLaunchSubscription)
			m_pLaunchSubscription->f_Clear();
		m_pLaunchSubscription.f_Clear();
		if (!m_Promise.f_IsSet())
			m_Promise.f_SetException(DMibErrorInstance("Aborted"));
	}


	TCFuture<void> CDistributedApp_LaunchInfoData::f_Destroy()
	{
		TCFuture<void> DestroyFuture;
		if (m_InProcess)
			DestroyFuture = m_InProcess->f_Destroy();
		else if (m_Launch)
			DestroyFuture = m_Launch->f_Destroy();
		else
			DestroyFuture = fg_Explicit();

		return DestroyFuture;
	}

#if DMibConfig_Tests_Enable
	TCFuture<NEncoding::CEJSON> CDistributedApp_LaunchInfoData::f_Test_Command(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params)
	{
		if (!m_InProcess)
			DMibError("No in process actor");
		return m_InProcess(&CDistributedAppInProcessActor::f_Test_Command, _Command, _Params);
	}

	TCFuture<uint32> CDistributedApp_LaunchInfoData::f_RunCommandLine
		(
			CCallingHostInfo const &_CallingHost
			, NStr::CStr const &_Command
			, NEncoding::CEJSON const &_Params
			, NStorage::TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		if (!m_InProcess)
			DMibError("No in process actor");
		return m_InProcess(&CDistributedAppInProcessActor::f_RunCommandLine, _CallingHost, _Command, _Params, _pCommandLine);
	}
#endif

	TCFuture<void> CDistributedApp_LaunchInfo::f_Destroy()
	{
		if (!m_Subscription)
			return CDistributedApp_LaunchInfoData::f_Destroy();

		TCPromise<void> Promise;
		
		auto Subscription = fg_Move(m_Subscription);

		Subscription->f_Destroy() > Promise / [Promise, This = fg_Move(*this)]() mutable
			{
				This.CDistributedApp_LaunchInfoData::f_Destroy() > Promise;
			}
		;

		return Promise.f_MoveFuture();
	}


	CDistributedApp_LaunchInfo::CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfoData const &_LaunchInfo, CActorSubscription &&_Subscription)
		: CDistributedApp_LaunchInfoData{_LaunchInfo}
		, m_Subscription(fg_Move(_Subscription))
	{
	}

	CDistributedApp_LaunchInfo::~CDistributedApp_LaunchInfo() = default;
	CDistributedApp_LaunchInfoData::CDistributedApp_LaunchInfoData() = default;
	CDistributedApp_LaunchInfoData::~CDistributedApp_LaunchInfoData() = default;
	
	NConcurrency::TCFuture<NConcurrency::TCActorSubscriptionWithID<>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_RegisterDistributedApp
		(
			NConcurrency::TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> &&_ClientInterface
			, NConcurrency::TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> &&_TrustInterface
			, CRegisterInfo const &_RegisterInfo
		)
	{
		auto *pThis = m_pThis;
		auto &CallingHostInfo = fg_GetCallingHostInfo();
		
		auto &HostID = CallingHostInfo.f_GetRealHostID();
		
		for (auto &LaunchInfo : pThis->m_Launches)
		{
			if (LaunchInfo.m_HostID != HostID)
				continue;

			auto &LaunchID = pThis->m_Launches.fs_GetKey(LaunchInfo);

			LaunchInfo.m_pClientInterface = fg_Construct(fg_Move(_ClientInterface));
			if (_TrustInterface)
				LaunchInfo.m_pTrustInterface = fg_Construct(fg_Move(_TrustInterface));

			DMibCallActor(*LaunchInfo.m_pClientInterface, CDistributedAppInterfaceClient::f_GetAppStartResult)
				> [LaunchID, pThis](TCAsyncResult<void> &&_LaunchResult)
				{
					auto *pLaunch = pThis->m_Launches.f_FindEqual(LaunchID);
					if (!pLaunch)
						return;

					auto &LaunchInfo = *pLaunch;

					if (LaunchInfo.m_Promise.f_IsSet())
						return;
					if (!_LaunchResult)
					{
						LaunchInfo.m_Promise.f_SetException(_LaunchResult);
						return;
					}
					LaunchInfo.m_Promise.f_SetResult
						(
							CDistributedApp_LaunchInfo
							{
								LaunchInfo
								, pThis->fp_GetLaunchSubscription(LaunchID)
							}
						)
					;
				}
			;

			return fg_Explicit(g_ActorSubscription / []{});
		}
		auto &PendingLaunch = pThis->m_PendingLaunches[HostID];
		PendingLaunch.m_pClientInterface = fg_Construct(fg_Move(_ClientInterface));
		if (_TrustInterface)
			PendingLaunch.m_pTrustInterface = fg_Construct(fg_Move(_TrustInterface));
		
		return fg_Explicit(g_ActorSubscription / []{});
	}

	CDistributedApp_LaunchHelper::CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr)
		: m_Dependencies(_Dependencies)
		, m_bLogToStderr(_bLogToStderr)
	{
		m_AppInterfaceServer.f_Publish<CDistributedAppInterfaceServer>(m_Dependencies.m_DistributionManager, this, CDistributedAppInterfaceServer::mc_pDefaultNamespace)
			> [](TCAsyncResult<void> &&_Result)
			{
				if (!_Result)
					DMibLog(Info, "Failed to publish app interface server: {}", _Result.f_GetExceptionStr());
			}
		;
	}
	
	CDistributedApp_LaunchHelper::~CDistributedApp_LaunchHelper()
	{
		for (auto &Launch : m_PendingLaunches)
		{
			if (Launch.m_pClientInterface)
				Launch.m_pClientInterface->f_Destroy() > fg_DiscardResult();
			if (Launch.m_pTrustInterface)
				Launch.m_pTrustInterface->f_Destroy() > fg_DiscardResult();
		}
	};

	TCFuture<void> CDistributedApp_LaunchHelper::fp_Destroy()
	{
		TCActorResultVector<void> Destroys;
		for (auto &Launch : m_Launches)
		{
			Launch.f_Destroy() > Destroys.f_AddResult();
			Launch.f_Abort();
		}
		
		for (auto &Destroy : m_PendingDestroys)
			Destroy > Destroys.f_AddResult();

		TCPromise<void> Promise;
		Destroys.f_GetResults() > [=](auto &&_Results)
			{
				m_AppInterfaceServer.f_Destroy() > Promise;
			}
		;
		return Promise.f_MoveFuture();
	}
	
	CActorSubscription CDistributedApp_LaunchHelper::fp_GetLaunchSubscription(NStr::CStr const &_LaunchID)
	{
		return g_ActorSubscription / [this, _LaunchID]() -> TCFuture<void>
			{
				auto *pLaunch = m_Launches.f_FindEqual(_LaunchID);
				if (!pLaunch)
				{
					DMibLog(Info, "Launch subscription goes out of scope: {} has no launch", _LaunchID);
					return fg_Explicit();
				}
				TCPromise<void> Promise;
				DMibLog(Info, "Launch subscription goes out of scope: {} {}", _LaunchID, pLaunch->m_HostID);
				pLaunch->f_Destroy() > [=, Pending = m_PendingDestroys[_LaunchID], HostID = pLaunch->m_HostID](auto &&_Result)
					{
						if (!_Result)
							DMibLog(Info, "Launch subscription Destroy failed: {} {}", _LaunchID, _Result.f_GetExceptionStr());

						DMibLog(Info, "Launch subscription Destroy finished: {} {}", _LaunchID, HostID);
						Pending.f_SetResult();
						Promise.f_SetResult();
						m_PendingDestroys.f_Remove(_LaunchID);
					}
				;
				m_Launches.f_Remove(_LaunchID);
				return Promise.f_MoveFuture();
			}
		;
	}

	TCFuture<CDistributedApp_LaunchInfo> CDistributedApp_LaunchHelper::f_LaunchInProcess
		(
			NStr::CStr const &_Description
			, NStr::CStr const &_HomeDirectory
			, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory
			, NContainer::TCVector<NStr::CStr> &&_Params
		)
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID();
		auto &LaunchInfo = m_Launches[LaunchID];
		auto Promise = LaunchInfo.m_Promise;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_InProcess = fg_ConstructActor<CDistributedAppInProcessActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor
				/ [this, LaunchID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCFuture<void>
				{
					auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
					DMibCheck(pLaunch);
					pLaunch->m_HostID = _HostID;
					return fg_Explicit();
				}
				, _Description
				, true
			)
		;

		LaunchInfo.m_InProcess(&CDistributedAppInProcessActor::f_Launch, _HomeDirectory, fg_Move(_fDistributedAppFactory), fg_Move(_Params))
			> Promise / [this, LaunchID](NStr::CStr &&_HostID)
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;
				pLaunch->m_HostID = _HostID;
				if (auto pPending = m_PendingLaunches.f_FindEqual(_HostID))
				{
					pLaunch->m_pClientInterface = fg_Move(pPending->m_pClientInterface);
					pLaunch->m_pTrustInterface = fg_Move(pPending->m_pTrustInterface);
					m_PendingLaunches.f_Remove(pPending);
					if (!pLaunch->m_Promise.f_IsSet())
					{
						pLaunch->m_Promise.f_SetResult
							(
								CDistributedApp_LaunchInfo
								{
									*pLaunch
									, fp_GetLaunchSubscription(LaunchID)
								}
							)
						;
					}
				}
			}
		;

		return Promise.f_MoveFuture();
	}

	auto CDistributedApp_LaunchHelper::f_Launch(NStr::CStr const &_Description, NStr::CStr const &_Executable)
		-> TCFuture<CDistributedApp_LaunchInfo>
	{
		return f_LaunchWithParams(_Description, _Executable, {});
	}
	
	auto CDistributedApp_LaunchHelper::f_LaunchWithParams(NStr::CStr const &_Description, NStr::CStr const &_Executable, NContainer::TCVector<NStr::CStr> &&_ExtraParams)
		-> TCFuture<CDistributedApp_LaunchInfo>
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID();
		auto &LaunchInfo = m_Launches[LaunchID];
		auto Promise = LaunchInfo.m_Promise;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_Launch = fg_ConstructActor<CDistributedAppInterfaceLaunchActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor 
				/ [this, LaunchID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCFuture<void>
				{
					auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
					DMibCheck(pLaunch);
					pLaunch->m_HostID = _HostID;
					return fg_Explicit();
				}
				, g_ActorFunctor / [](NStr::CStr const &_Error) -> TCFuture<void>
				{
					return fg_Explicit();
				}
				, _Description
				, true
			)
		;
		
		auto Params = NContainer::fg_CreateVector<NStr::CStr>("--daemon-run-standalone");

		if (m_bLogToStderr)
			Params.f_Insert("--log-to-stderr");
		Params.f_Insert(fg_Move(_ExtraParams));
		
		NProcess::CProcessLaunchActor::CLaunch Launch
			{
				NProcess::CProcessLaunchParams::fs_LaunchExecutable
				(
#ifdef DPlatformFamily_Windows
					_Executable + ".exe"
#else
					_Executable
#endif
					, fg_Move(Params)
					, NFile::CFile::fs_GetPath(_Executable)
					, [Promise](NProcess::CProcessLaunchStateChangeVariant const &_State, fp64 _TimeSinceStart)
					{
						switch (_State.f_GetTypeID())
						{
						case NProcess::EProcessLaunchState_LaunchFailed:
							{
								if (!Promise.f_IsSet())
									Promise.f_SetException(DMibErrorInstance(NStr::fg_Format("Launch failed: {}", _State.f_Get<NProcess::EProcessLaunchState_LaunchFailed>())));
								break;
							}
						case NProcess::EProcessLaunchState_Exited:
							{
								if (!Promise.f_IsSet())
									Promise.f_SetException(DMibErrorInstance(NStr::fg_Format("Launch exited unexpectedly: {}", _State.f_Get<NProcess::EProcessLaunchState_Exited>())));
								break;
							}
						case NProcess::EProcessLaunchState_Launched:
							break;
						}
					}
				)
			}
		;
		
		if (m_bLogToStderr)
			Launch.m_ToLog = NProcess::CProcessLaunchActor::ELogFlag_All | NProcess::CProcessLaunchActor::ELogFlag_AdditionallyOutputToStdErr;
			
#ifdef DPlatformFamily_Windows
		Launch.m_Params.m_bCreateNewProcessGroup = true;
#endif
		
		LaunchInfo.m_Launch(&NProcess::CProcessLaunchActor::f_Launch, Launch, fg_ThisActor(this)) > Promise / [this, LaunchID](NConcurrency::CActorSubscription &&_Subscription)
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;
				pLaunch->m_pLaunchSubscription = fg_Construct(fg_Move(_Subscription));
			}
		;
		
		return Promise.f_MoveFuture();
	}
}
