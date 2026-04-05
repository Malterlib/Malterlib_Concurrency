// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Core/Core>
#include <Mib/Cryptography/RandomID>
#include <Mib/Concurrency/ActorSubscription>
#include <Mib/Encoding/EJson>

#include "Malterlib_Concurrency_DistributedAppInterfaceLaunch.h"

namespace NMib::NConcurrency
{
	CDistributedAppInterfaceLaunchActor::CDistributedAppInterfaceLaunchActor
		(
			NWeb::NHTTP::CURL const &_Address
			, TCActor<CDistributedActorTrustManager> const &_TrustManager
			, FOnUseTicket &&_fOnUseTicket
			, TCActorFunctor<TCFuture<void> (NStr::CStr _Error)> &&_fOnLaunchError
			, NStr::CStr const &_Description
			, NStr::CStr const &_LaunchID
			, bool _bDelegateTrust
		)
		: mp_Address(_Address)
		, mp_TrustManager(_TrustManager)
		, mp_fOnUseTicket(fg_Move(_fOnUseTicket))
		, mp_fOnLaunchError(fg_Move(_fOnLaunchError))
		, mp_RequestTicketMagic(NCryptography::fg_RandomID())
		, mp_Description(_Description)
		, mp_LaunchID(_LaunchID)
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
		LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerLaunchID"] = mp_LaunchID;
		if (mp_bDelegateTrust)
			LaunchParams.m_Environment["MalterlibDistributedAppInterfaceServerOptions"] = "DelegateTrust";

		LaunchParams.m_Environment["MalterlibProtectedEnvironment"] =
			"MalterlibDistributedAppInterfaceServerAddress"
			";MalterlibDistributedAppInterfaceServerRequestTicket"
			";MalterlibDistributedAppInterfaceServerOptions"
			";MalterlibDistributedAppInterfaceServerLaunchID"
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
							NEncoding::CEJsonSorted const Data = NEncoding::CEJsonSorted::fs_FromString(CommandData);
							if (mp_fOnLaunchError)
								mp_fOnLaunchError.f_CallDiscard(Data["Error"].f_String());
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

		NStr::CStr HandleRequestID = NCryptography::fg_RandomID(mp_HandleRequests);

		mp_TrustManager
			(
				&CDistributedActorTrustManager::f_GenerateConnectionTicket
				, mp_Address
				, g_ActorFunctor
				(
					g_ActorSubscription / [this, HandleRequestID]() -> TCFuture<void>
					{
						auto pHandleRequest = mp_HandleRequests.f_FindEqual(HandleRequestID);
						if (!pHandleRequest)
							co_return {};

						if (pHandleRequest->m_NotificationsSubscription)
							co_await pHandleRequest->m_NotificationsSubscription->f_Destroy();

						mp_HandleRequests.f_Remove(HandleRequestID);

						co_return {};
					}
				)
				/ [this, HandleRequestID](NStr::CStr _HostID, CCallingHostInfo _HostInfo, NContainer::CByteVector _CertificateRequest) -> TCFuture<void>
				{
					co_await mp_fOnUseTicket(_HostID, _HostInfo, _CertificateRequest);

					mp_HandleRequests.f_Remove(HandleRequestID);

					co_return {};
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
