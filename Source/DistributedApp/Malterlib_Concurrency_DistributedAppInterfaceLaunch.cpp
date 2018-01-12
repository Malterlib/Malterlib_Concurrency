// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/ActorSubscription>

#include "Malterlib_Concurrency_DistributedAppInterfaceLaunch.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceLaunchActor::CDistributedAppInterfaceLaunchActor
		(
			NHTTP::CURL const &_Address
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, FOnUseTicket &&_fOnUseTicket
			, NStr::CStr const &_Description
			, bool _bDelegateTrust
		)
		: mp_Address(_Address)
		, mp_TrustManager(_TrustManager)
		, mp_fOnUseTicket(fg_Move(_fOnUseTicket))
		, mp_RequestTicketMagic(NCryptography::fg_RandomID())
		, mp_Description(_Description)
		, mp_bDelegateTrust(_bDelegateTrust)
	{
		mp_RequestTicketMagicLine = mp_RequestTicketMagic + "\n";
	}
	
	CDistributedAppInterfaceLaunchActor::~CDistributedAppInterfaceLaunchActor()
	{
	}
	
	bool CDistributedAppInterfaceLaunchActor::fp_WillFilterOutput()
	{
		return true;
	}
	
	void CDistributedAppInterfaceLaunchActor::fp_ModifyLaunch(CLaunch &o_Launch)
	{
		o_Launch.m_bWholeLineOutput = true;
		auto &LaunchParams = o_Launch.m_Params;
		if (LaunchParams.m_Environment.f_IsEmpty())
			LaunchParams.m_bMergeEnvironment = true;
		LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerAddress"] = mp_Address.f_Encode();
		LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerRequestTicket"] = mp_RequestTicketMagic;
		if (mp_bDelegateTrust)
			LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerOptions"] = "DelegateTrust";
		
		LaunchParams.m_Environment["MalterlibProtectedEnvironment"] 
			= "MalterlibDistributedAppInterfaceServerAddress;MalterlibDistributedAppInterfaceServerRequestTicket;MalterlibDistributedAppInterfaceServerOptions"
		;
	}
	
	void CDistributedAppInterfaceLaunchActor::fp_FilterOutput(NProcess::EProcessLaunchOutputType _OutputType, NStr::CStr &o_Output)
	{
		if (o_Output.f_IsEmpty())
			return;
		if (_OutputType == NProcess::EProcessLaunchOutputType_StdErr && o_Output.f_Find(mp_RequestTicketMagicLine) >= 0)
		{
			o_Output = o_Output.f_Replace(mp_RequestTicketMagicLine, "");
			fp_HandleTicketRequest();
		}
	}
	
	void CDistributedAppInterfaceLaunchActor::fp_HandleTicketRequest()
	{
		DMibLogWithCategory(Malterlib/Concurrency, Info, "Generating ticket for '{}'", mp_Description);
		
		NStr::CStr HandleRequestID = NCryptography::fg_RandomID();
		
		mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, g_ActorFunctor
				(
					g_ActorSubscription > [this, HandleRequestID]() -> TCContinuation<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							return fg_Explicit();
						
						TCContinuation<void> Continuation;
						if (pHandleRequest->m_OnUseTicketSubscription)
							Continuation = pHandleRequest->m_OnUseTicketSubscription->f_Destroy();
						else
							Continuation.f_SetResult();
						
						mp_HandleRequests.f_Remove(HandleRequestID);
						
						return Continuation;
					}
				)
				> [this, HandleRequestID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::TCVector<uint8> const &_CertificateRequest) -> TCContinuation<void>
				{
					TCContinuation<void> Continuation;
					mp_fOnUseTicket(_HostID, _HostInfo, _CertificateRequest) > [this, HandleRequestID, Continuation](TCAsyncResult<void> &&_Result)
						{
							mp_HandleRequests.f_Remove(HandleRequestID);
							Continuation.f_SetResult(fg_Move(_Result));
						}
					;
					return Continuation;
				}
			)
			> [this, HandleRequestID](TCAsyncResult<CDistributedActorTrustManager::CTrustGenerateConnectionTicketResult> &&_Ticket)
			{
				if (!_Ticket)
				{
					DMibLogWithCategory
						(
							Malterlib/Concurrency
							, Error
							, "Failed to generate ticket for '{}': {}"
							, mp_Description
							, _Ticket.f_GetExceptionStr()
						)
					;
					return;
				}
				auto &Request = mp_HandleRequests[HandleRequestID];
				Request.m_OnUseTicketSubscription = fg_Move(_Ticket->m_OnUseTicketSubscription);
				DMibLogWithCategory(Malterlib/Concurrency, Info, "Sending ticket to '{}'", mp_Description);
				CProcessLaunchActor::f_SendStdIn(mp_RequestTicketMagic + ":" + _Ticket->m_Ticket.f_ToStringTicket() + "\n") > [this](TCAsyncResult<void> &&_Result)
					{
						(void)this;
						if (!_Result)
							DMibLogWithCategory(Malterlib/Concurrency, Error, "Failed to send ticket to '{}'", mp_Description);
					}
				;
			}
		;
	}
}
