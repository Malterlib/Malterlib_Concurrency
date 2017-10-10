// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Concurrency/ActorSubscription>

#include "Malterlib_Concurrency_DistributedApp_TestHelpers.h"

namespace NMib::NConcurrency
{
	void CDistributedApp_LaunchInfo::f_Abort()
	{
		m_Launch.f_Clear();
		m_InProcess.f_Clear();
		if (m_pClientInterface)
			m_pClientInterface->f_Clear();
		m_pClientInterface.f_Clear();
		if (m_pLaunchSubscription)
			m_pLaunchSubscription->f_Clear();
		m_pLaunchSubscription.f_Clear();
		if (!m_Continuation.f_IsSet())
			m_Continuation.f_SetException(DMibErrorInstance("Aborted"));
	}

	TCContinuation<void> CDistributedApp_LaunchInfo::f_Destroy()
	{
		if (m_InProcess)
			return m_InProcess->f_Destroy();
		else if (m_Launch)
			return m_Launch->f_Destroy();
		return fg_Explicit();
	}

	
	CDistributedApp_LaunchInfo::CDistributedApp_LaunchInfo() = default;
	CDistributedApp_LaunchInfo::~CDistributedApp_LaunchInfo() = default;
	
	NConcurrency::TCContinuation<NConcurrency::TCActorSubscriptionWithID<>> CDistributedApp_LaunchHelper::CDistributedAppInterfaceServerImplementation::f_RegisterDistributedApp
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

			LaunchInfo.m_pClientInterface = fg_Construct(fg_Move(_ClientInterface));
			if (_TrustInterface)
				LaunchInfo.m_pTrustInterface = fg_Construct(fg_Move(_TrustInterface));
			if (!LaunchInfo.m_Continuation.f_IsSet())
				LaunchInfo.m_Continuation.f_SetResult(LaunchInfo);
			
			return fg_Explicit(g_ActorSubscription > []{});
		}
		
		return fg_Explicit(nullptr);
	}

	CDistributedApp_LaunchHelper::CDistributedApp_LaunchHelper(CDistributedApp_LaunchHelperDependencies const &_Dependencies, bool _bLogToStderr)
		: m_Dependencies(_Dependencies)
		, m_bLogToStderr(_bLogToStderr)
	{
		m_AppInterfaceServer.f_Publish<CDistributedAppInterfaceServer>(m_Dependencies.m_DistributionManager, this, CDistributedAppInterfaceServer::mc_pDefaultNamespace);
	}
	
	CDistributedApp_LaunchHelper::~CDistributedApp_LaunchHelper() = default;

	TCContinuation<void> CDistributedApp_LaunchHelper::fp_Destroy()
	{
		TCActorResultVector<void> Destroys;
		for (auto &Launch : m_Launches)
		{
			Launch.f_Destroy() > Destroys.f_AddResult();
			Launch.f_Abort();
		}

		m_AppInterfaceServer.f_Destroy() > Destroys.f_AddResult();
		
		TCContinuation<void> Continuation;
		Destroys.f_GetResults() > Continuation.f_ReceiveAny();
		return Continuation;
	}

	TCContinuation<CDistributedApp_LaunchInfo> CDistributedApp_LaunchHelper::f_LaunchInProcess(NStr::CStr const &_Description, NStr::CStr const &_HomeDirectory, NFunction::TCFunction<TCActor<CDistributedAppActor> ()> &&_fDistributedAppFactory)
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID();
		auto &LaunchInfo = m_Launches[LaunchID];
		auto Continuation = LaunchInfo.m_Continuation;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_InProcess = fg_ConstructActor<CDistributedAppInProcessActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor
				> [this, LaunchID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::TCVector<uint8> const &_CertificateRequest) -> TCContinuation<void>
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

		LaunchInfo.m_InProcess(&CDistributedAppInProcessActor::f_Launch, _HomeDirectory, fg_Move(_fDistributedAppFactory)) > Continuation / [this, LaunchID]()
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;
			}
		;

		return Continuation;
	}

	auto CDistributedApp_LaunchHelper::f_Launch(NStr::CStr const &_Description, NStr::CStr const &_Executable)
		-> TCContinuation<CDistributedApp_LaunchInfo>
	{
		return f_LaunchWithParams(_Description, _Executable, {});
	}
	
	auto CDistributedApp_LaunchHelper::f_LaunchWithParams(NStr::CStr const &_Description, NStr::CStr const &_Executable, NContainer::TCVector<NStr::CStr> &&_ExtraParams)
		-> TCContinuation<CDistributedApp_LaunchInfo>
	{
		NStr::CStr LaunchID = NCryptography::fg_RandomID();
		auto &LaunchInfo = m_Launches[LaunchID];
		auto Continuation = LaunchInfo.m_Continuation;
		LaunchInfo.m_LaunchID = LaunchID;
		LaunchInfo.m_Launch = fg_ConstructActor<CDistributedAppInterfaceLaunchActor>
			(
				m_Dependencies.m_Address
				, m_Dependencies.m_TrustManager
				, g_ActorFunctor 
				> [this, LaunchID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::TCVector<uint8> const &_CertificateRequest) -> TCContinuation<void>
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
					, [Continuation](NProcess::CProcessLaunchStateChangeVariant const &_State, fp64 _TimeSinceStart)
					{
						switch (_State.f_GetTypeID())
						{
						case NProcess::EProcessLaunchState_LaunchFailed:
							{
								if (!Continuation.f_IsSet())
									Continuation.f_SetException(DMibErrorInstance(NStr::fg_Format("Launch failed: {}", _State.f_Get<NProcess::EProcessLaunchState_LaunchFailed>())));
								break;
							}
						case NProcess::EProcessLaunchState_Exited:
							{
								if (!Continuation.f_IsSet())
									Continuation.f_SetException(DMibErrorInstance(NStr::fg_Format("Launch exited unexpectedly: {}", _State.f_Get<NProcess::EProcessLaunchState_Exited>())));
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
		
		LaunchInfo.m_Launch(&NProcess::CProcessLaunchActor::f_Launch, Launch, fg_ThisActor(this)) > Continuation / [this, LaunchID](NConcurrency::CActorSubscription &&_Subscription)
			{
				auto *pLaunch = m_Launches.f_FindEqual(LaunchID);
				if (!pLaunch)
					return;
				pLaunch->m_pLaunchSubscription = fg_Construct(fg_Move(_Subscription));
			}
		;
		
		return Continuation;
	}
}
