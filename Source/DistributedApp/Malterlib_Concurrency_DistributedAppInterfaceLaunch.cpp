// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>

#include "Malterlib_Concurrency_DistributedAppInterfaceLaunch.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceLaunchActor::CDistributedAppInterfaceLaunchActor
		(
			NHTTP::CURL const &_Address
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, FOnUseTicket &&_fOnUseTicket
			, NStr::CStr const &_Description
		)
		: mp_Address(_Address)
		, mp_TrustManager(_TrustManager)
		, mp_fOnUseTicket(fg_Move(_fOnUseTicket))
		, mp_RequestTicketMagic(NCryptography::fg_RandomID())
		, mp_Description(_Description)
	{
		mp_RequestTicketMagicLine = mp_RequestTicketMagic + "\n";
	}
	
	CDistributedAppInterfaceLaunchActor::~CDistributedAppInterfaceLaunchActor()
	{
	}
	
	void CDistributedAppInterfaceLaunchActor::f_ModifyLaunch(CLaunch &o_Launch)
	{
		o_Launch.m_bWholeLineOutput = true;
		auto &LaunchParams = o_Launch.m_Params;
		if (LaunchParams.m_Environment.f_IsEmpty())
			LaunchParams.m_bMergeEnvironment = true;
		LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerAddress"] = mp_Address.f_Encode();
		LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerRequestTicket"] = mp_RequestTicketMagic;
		LaunchParams.m_Environment["MalterlibProtectedEnvironment"] = "MalterlibDistributedAppInterfaceServerAddress;MalterlibDistributedAppInterfaceServerRequestTicket";
	}
	
	void CDistributedAppInterfaceLaunchActor::f_FilterOutput(NProcess::EProcessLaunchOutputType _OutputType, NStr::CStr &o_Output)
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
		
		mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, fg_Move(mp_fOnUseTicket)
			)
			> [this](TCAsyncResult<CDistributedActorTrustManager::CTrustTicket> &&_Ticket)
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
				DMibLogWithCategory(Malterlib/Concurrency, Info, "Sending ticket to '{}'", mp_Description);
				CProcessLaunchActor::f_SendStdIn(mp_RequestTicketMagic + ":" + _Ticket->f_ToStringTicket() + "\n") > [this](TCAsyncResult<void> &&_Result)
					{
						if (!_Result)
							DMibLogWithCategory(Malterlib/Concurrency, Error, "Failed to send ticket to '{}'", mp_Description);
					}
				;
			}
		;
	}
}
