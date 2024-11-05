// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Concurrency/LogError>

#include "Malterlib_Concurrency_DistributedApp_LaunchHelper.h"

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
		if (m_InProcess)
			return fg_Move(m_InProcess).f_Destroy();
		else if (m_Launch)
			return fg_Move(m_Launch).f_Destroy();
		else
			return g_Void;
	}

#if DMibConfig_Tests_Enable
	TCFuture<NEncoding::CEJSONSorted> CDistributedApp_LaunchInfoData::f_Test_Command(NStr::CStr &&_Command, NEncoding::CEJSONSorted &&_Params)
	{
		if (!m_InProcess)
			return DMibErrorInstance("No in process actor");
		return m_InProcess(&CDistributedAppInProcessActor::f_Test_Command, fg_Move(_Command), fg_Move(_Params));
	}

	TCFuture<uint32> CDistributedApp_LaunchInfoData::f_RunCommandLine
		(
			CCallingHostInfo &&_CallingHost
			, NStr::CStr &&_Command
			, NEncoding::CEJSONSorted &&_Params
			, NStorage::TCSharedPointer<CCommandLineControl> &&_pCommandLine
		)
	{
		if (!m_InProcess)
			return DMibErrorInstance("No in process actor");

		return m_InProcess(&CDistributedAppInProcessActor::f_RunCommandLine, fg_Move(_CallingHost), fg_Move(_Command), fg_Move(_Params), fg_Move(_pCommandLine));
	}
#endif

	TCFuture<void> CDistributedApp_LaunchInfo::f_Destroy()
	{
		if (!m_Subscription)
			return CDistributedApp_LaunchInfoData::f_Destroy();

		TCPromiseFuturePair<void> Promise;

		auto Subscription = fg_Move(m_Subscription);

		Subscription->f_Destroy() > fg_Move(Promise.m_Promise) / [Promise = Promise.m_Promise, This = fg_Move(*this)]() mutable
			{
				This.CDistributedApp_LaunchInfoData::f_Destroy() > [Promise = fg_Move(Promise)](TCAsyncResult<void> &&)
					{
						Promise.f_SetResult();
					}
				;
			}
		;

		return fg_Move(Promise.m_Future);
	}


	CDistributedApp_LaunchInfo::CDistributedApp_LaunchInfo(CDistributedApp_LaunchInfoData const &_LaunchInfo, CActorSubscription &&_Subscription)
		: CDistributedApp_LaunchInfoData{_LaunchInfo}
		, m_Subscription(fg_Move(_Subscription))
	{
	}

	CDistributedApp_LaunchInfo::~CDistributedApp_LaunchInfo() = default;
	CDistributedApp_LaunchInfoData::CDistributedApp_LaunchInfoData() = default;
	CDistributedApp_LaunchInfoData::~CDistributedApp_LaunchInfoData() = default;
	
	TCFuture<TCActorSubscriptionWithID<>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_RegisterDistributedApp
		(
			TCDistributedActorInterfaceWithID<CDistributedAppInterfaceClient> _ClientInterface
			, TCDistributedActorInterfaceWithID<CDistributedActorTrustManagerInterface> _TrustInterface
			, CRegisterInfo _RegisterInfo
		)
	{
		if (!_ClientInterface)
			co_return DMibErrorInstance("Invalid client interface");

		auto *pThis = m_pThis;
		auto &CallingHostInfo = fg_GetCallingHostInfo();
		
		auto &HostID = CallingHostInfo.f_GetRealHostID();
		NStr::CStr LaunchID;
		if (_RegisterInfo.m_LaunchID)
			LaunchID = *_RegisterInfo.m_LaunchID;
		
		for (auto &LaunchInfo : pThis->m_Launches)
		{
			if (LaunchInfo.m_HostID != HostID && LaunchInfo.m_LaunchID != LaunchID)
				continue;

			auto &LaunchID = pThis->m_Launches.fs_GetKey(LaunchInfo);

			LaunchInfo.m_HostID = HostID;

			LaunchInfo.m_pClientInterface = fg_Construct(fg_Move(_ClientInterface));
			if (_TrustInterface)
				LaunchInfo.m_pTrustInterface = fg_Construct(fg_Move(_TrustInterface));

			LaunchInfo.m_pClientInterface->f_CallActor(&CDistributedAppInterfaceClient::f_GetAppStartResult)()
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

			co_return g_ActorSubscription / []{};
		}
		auto &PendingLaunch = pThis->m_PendingLaunches[HostID];
		PendingLaunch.m_pClientInterface = fg_Construct(fg_Move(_ClientInterface));
		if (_TrustInterface)
			PendingLaunch.m_pTrustInterface = fg_Construct(fg_Move(_TrustInterface));
		
		co_return g_ActorSubscription / []{};
	}

	TCFuture<TCActorSubscriptionWithID<>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_RegisterConfigFiles(CConfigFiles _ConfigFiles)
	{
		co_return g_ActorSubscription / []{};
	}

	TCFuture<TCDistributedActorInterfaceWithID<CDistributedAppSensorReporter>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_GetSensorReporter()
	{
		co_return {};
	}

	TCFuture<TCDistributedActorInterfaceWithID<CDistributedAppLogReporter>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_GetLogReporter()
	{
		co_return {};
	}

	CDistributedApp_LaunchHelper::CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr)
		: m_Dependencies(_Dependencies)
		, m_bLogToStderr(_bLogToStderr)
	{
		m_AppInterfaceServer.f_Publish<CDistributedAppInterfaceServer>(m_Dependencies.m_DistributionManager, this)
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
				Launch.m_pClientInterface->f_Destroy().f_DiscardResult();
			if (Launch.m_pTrustInterface)
				Launch.m_pTrustInterface->f_Destroy().f_DiscardResult();
		}
	};

	TCFuture<void> CDistributedApp_LaunchHelper::fp_Destroy()
	{
		TCFutureVector<void> Destroys;
		for (auto &Destroy : m_PendingDestroys)
			Destroy.f_MoveFuture() > Destroys;
		m_PendingDestroys.f_Clear();

		CLogError LogError("DistributedAppLaunchHelper");

		co_await fg_AllDone(Destroys).f_Wrap() > LogError.f_Warning("Failed to destroy pending destroys");

		while (auto pLaunch = m_OrderdedLaunches.f_Pop())
		{
			auto Future = pLaunch->f_Destroy();
			pLaunch->f_Abort();
			co_await fg_Move(Future).f_Wrap() > LogError.f_Warning("Failed to destroy ordered launch");
		}

		co_await m_AppInterfaceServer.f_Destroy().f_Wrap() > LogError.f_Warning("Failed to destroy app interface server");

		co_return {};
	}
	
	CActorSubscription CDistributedApp_LaunchHelper::fp_GetLaunchSubscription(NStr::CStr const &_LaunchID)
	{
		return g_ActorSubscription / [this, _LaunchID]() -> TCFuture<void>
			{
				auto *pLaunch = m_Launches.f_FindEqual(_LaunchID);
				if (!pLaunch)
				{
					DMibLog(Info, "Launch subscription goes out of scope: {} has no launch", _LaunchID);
					co_return {};
				}
				DMibLog(Info, "Launch subscription goes out of scope: {} {}", _LaunchID, pLaunch->m_HostID);
				auto Result = co_await pLaunch->f_Destroy().f_Wrap();

				auto Pending = m_PendingDestroys[_LaunchID];
				auto HostID = pLaunch->m_HostID;

				if (!Result)
					DMibLog(Info, "Launch subscription destroy failed: {} {}", _LaunchID, Result.f_GetExceptionStr());

				DMibLog(Info, "Launch subscription destroy finished: {} {}", _LaunchID, HostID);

				Pending.f_SetResult();

				m_PendingDestroys.f_Remove(_LaunchID);

				m_Launches.f_Remove(_LaunchID);

				co_return {};
			}
		;
	}

	TCFuture<CDistributedApp_LaunchInfo> CDistributedApp_LaunchHelper::f_LaunchInProcess
		(
			NStr::CStr _Description
			, NStr::CStr _HomeDirectory
			, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> _fDistributedAppFactory
			, NContainer::TCVector<NStr::CStr> _Params
		)
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID(m_Launches);
		auto &LaunchInfo = m_Launches[LaunchID];
		m_OrderdedLaunches.f_InsertFirst(LaunchInfo);
		auto Promise = LaunchInfo.m_Promise;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_InProcess = fg_ConstructActor<CDistributedAppInProcessActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor
				/ [this, LaunchID](NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest) -> TCFuture<void>
				{
					auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
					DMibCheck(pLaunch);
					pLaunch->m_HostID = _HostID;
					co_return {};
				}
				, _Description
				, true
			)
		;

		LaunchInfo.m_InProcess(&CDistributedAppInProcessActor::f_Launch, _HomeDirectory, fg_Move(_fDistributedAppFactory), fg_Move(_Params))
			> [this, LaunchID](TCAsyncResult<NStr::CStr> &&_HostID)
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;

				if (!_HostID)
				{
					if (!pLaunch->m_Promise.f_IsSet())
						pLaunch->m_Promise.f_SetException(_HostID);
					return;
				}

				pLaunch->m_HostID = *_HostID;
				if (auto pPending = m_PendingLaunches.f_FindEqual(*_HostID))
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

		co_return co_await Promise.f_MoveFuture();
	}

	auto CDistributedApp_LaunchHelper::f_Launch(NStr::CStr _Description, NStr::CStr _Executable)
		-> TCFuture<CDistributedApp_LaunchInfo>
	{
		return f_LaunchWithParams(_Description, _Executable, {}, {});
	}

	auto CDistributedApp_LaunchHelper::f_LaunchWithParams
		(
			NStr::CStr _Description
			, NStr::CStr _Executable
			, NContainer::TCVector<NStr::CStr> _ExtraParams
			, CSystemEnvironment _Environment
		)
		-> TCFuture<CDistributedApp_LaunchInfo>
	{
		NContainer::TCVector<NStr::CStr> Params;

		if (_ExtraParams.f_IsEmpty() || !_ExtraParams[0].f_StartsWith("--daemon-run-"))
			Params = NContainer::fg_CreateVector<NStr::CStr>("--daemon-run-standalone");

		Params.f_Insert(fg_Move(_ExtraParams));
		if (m_bLogToStderr)
			Params.f_Insert("--log-to-stderr");

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
					, nullptr
				)
			}
		;

		if (m_bLogToStderr)
			Launch.m_ToLog = NProcess::CProcessLaunchActor::ELogFlag_All | NProcess::CProcessLaunchActor::ELogFlag_AdditionallyOutputToStdErr;

		Launch.m_Params.m_bCreateNewProcessGroup = true;
		Launch.m_Params.m_Environment = fg_Move(_Environment);

		return f_LaunchWithLaunch(_Description, fg_Move(Launch), nullptr);
	}

	TCFuture<CDistributedApp_LaunchInfo> CDistributedApp_LaunchHelper::f_LaunchWithLaunch
		(
			NStr::CStr _Description
			, NProcess::CProcessLaunchActor::CLaunch _Launch
			, TCActor<CActor> _NotificationActor
		)
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID(m_Launches);
		auto &LaunchInfo = m_Launches[LaunchID];
		m_OrderdedLaunches.f_InsertFirst(LaunchInfo);
		auto Promise = LaunchInfo.m_Promise;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_Launch = fg_ConstructActor<CDistributedAppInterfaceLaunchActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor
				/ [this, LaunchID](NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest) -> TCFuture<void>
				{
					auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
					DMibCheck(pLaunch);
					pLaunch->m_HostID = _HostID;
					co_return {};
				}
				, g_ActorFunctor / [](NStr::CStr _Error) -> TCFuture<void>
				{
					co_return {};
				}
				, _Description
				, LaunchID
				, true
			)
		;

		NProcess::CProcessLaunchActor::CLaunch Launch = fg_Move(_Launch);

		Launch.m_Params.m_fOnStateChange = [Promise, fOriginalOnStateChange = fg_Move(Launch.m_Params.m_fOnStateChange), _NotificationActor]
			(NProcess::CProcessLaunchStateChangeVariant const &_State, fp64 _TimeSinceStart)
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

				if (fOriginalOnStateChange)
				{
					fg_Dispatch
						(
							_NotificationActor
							, NFunction::TCFunctionMovable<void ()>
							(
								[_State, _TimeSinceStart, fOriginalOnStateChange]
								{
									fOriginalOnStateChange(_State, _TimeSinceStart);
								}
							)
						)
						.f_DiscardResult()
					;
				}
			}
		;

		if (Launch.m_Params.m_fOnOutput)
		{
			Launch.m_Params.m_fOnOutput = [Promise, fOriginalOnOutput = fg_Move(Launch.m_Params.m_fOnOutput), _NotificationActor]
				(NProcess::EProcessLaunchOutputType _OutputType, NStr::CStr const &_Output)
				{
					fg_Dispatch
						(
							_NotificationActor
							, NFunction::TCFunctionMovable<void ()>
							(
								[_OutputType, _Output, fOriginalOnOutput]
								{
									fOriginalOnOutput(_OutputType, _Output);
								}
							)
						)
						.f_DiscardResult()
					;
				}
			;
		}

		LaunchInfo.m_Launch(&NProcess::CProcessLaunchActor::f_Launch, Launch, fg_ThisActor(this)) > Promise / [this, LaunchID](CActorSubscription &&_Subscription)
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;
				pLaunch->m_pLaunchSubscription = fg_Construct(fg_Move(_Subscription));
			}
		;

		co_return co_await Promise.f_MoveFuture();
	}
}
