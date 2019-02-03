// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Encoding/EJSON>

#include "Malterlib_Concurrency_DistributedAppInterfaceLaunch.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceLaunchActor::CDistributedAppInterfaceLaunchActor
		(
			NWeb::NHTTP::CURL const &_Address
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, FOnUseTicket &&_fOnUseTicket
		 	, TCActorFunctor<TCContinuation<void> (NStr::CStr const &_Error)> &&_fOnLaunchError
			, NStr::CStr const &_Description
			, bool _bDelegateTrust
		)
		: mp_Address(_Address)
		, mp_TrustManager(_TrustManager)
		, mp_fOnUseTicket(fg_Move(_fOnUseTicket))
		, mp_fOnLaunchError(fg_Move(_fOnLaunchError))
		, mp_RequestTicketMagic(NCryptography::fg_RandomID())
		, mp_Description(_Description)
		, mp_bDelegateTrust(_bDelegateTrust)
	{
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
		if (_OutputType == NProcess::EProcessLaunchOutputType_StdErr)
		{
			auto iRequestMagic = o_Output.f_Find(mp_RequestTicketMagic);
			while (iRequestMagic >= 0)
			{
				auto iOutputLen = o_Output.f_GetLen();
				ch8 const *pOutput = o_Output.f_GetStr();

				auto iEndLine = iRequestMagic + mp_RequestTicketMagic.f_GetLen();
				auto iStartCommand = iEndLine;

				while (iEndLine < iOutputLen && pOutput[iEndLine] != '\n')
					++iEndLine;

				if (pOutput[iStartCommand] == ':')
					++iStartCommand;

				NStr::CStr CommandData = o_Output.f_Extract(iStartCommand, iEndLine - iStartCommand);

				if (pOutput[iEndLine] == '\n')
					++iEndLine;

				o_Output = o_Output.f_Delete(iRequestMagic, iEndLine - iRequestMagic);

				if (CommandData.f_IsEmpty())
					fp_HandleTicketRequest();
				else
				{
					NStr::CStr Command = NStr::fg_GetStrSep(CommandData, ":");
					if (Command == "Error")
					{
						try
						{
							NEncoding::CEJSON const Data = NEncoding::CEJSON::fs_FromString(CommandData);
							if (mp_fOnLaunchError)
								mp_fOnLaunchError(Data["Error"].f_String()) > fg_DiscardResult();
						}
						catch (NException::CException const &_Exception)
						{
							(void)_Exception;
							DMibLogWithCategory(Malterlib/Concurrency, Info, "For '{}', failed to parse error command: {}", mp_Description, _Exception);
						}
					}
					else
						DMibLogWithCategory(Malterlib/Concurrency, Info, "For '{}', unknown DistributedAppInterface command: {}", mp_Description, Command);
				}
				iRequestMagic = o_Output.f_Find(mp_RequestTicketMagic);
			}
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
					g_ActorSubscription / [this, HandleRequestID]() -> TCContinuation<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							return fg_Explicit();
						
						TCContinuation<void> Continuation;
						if (pHandleRequest->m_NotificationsSubscription)
							Continuation = pHandleRequest->m_NotificationsSubscription->f_Destroy();
						else
							Continuation.f_SetResult();
						
						mp_HandleRequests.f_Remove(HandleRequestID);
						
						return Continuation;
					}
				)
				/ [this, HandleRequestID](NStr::CStr const &_HostID, CCallingHostInfo const &_HostInfo, NContainer::CByteVector const &_CertificateRequest) -> TCContinuation<void>
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
			 	, nullptr
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
				Request.m_NotificationsSubscription = fg_Move(_Ticket->m_NotificationsSubscription);
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
