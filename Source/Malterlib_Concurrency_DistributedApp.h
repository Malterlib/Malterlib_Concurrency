// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJSONDatabase>
#include <Mib/Concurrency/ActorCallOnce>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLineClient.h"

namespace NMib
{
	namespace NConcurrency
	{
		struct CDistributedAppActor_Settings
		{
			CDistributedAppActor_Settings() = default;
			CDistributedAppActor_Settings
				(
					NStr::CStr const &_AppName
					, bool _bRequireListen
					, NStr::CStr const &_ConfigDirectory = NFile::CFile::fs_GetProgramDirectory()
					, bool _bSeparateConcurrencyManager = false
					, uint32 _KeySize = 4096
					, NStr::CStr const &_FriendlyName = NStr::CStr()
				)
			;
			NStr::CStr f_GetCompositeFriendlyName() const;
			
			NStr::CStr m_AppName;
			NStr::CStr m_ConfigDirectory;
			NStr::CStr m_FriendlyName;
			bool m_bRequireListen = false;
			bool m_bSeparateConcurrencyManager = false;
			uint32 m_KeySize = 4096;
			NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
		};
		
		struct CDistributedAppActor;
		
		struct CDistributedAppState
		{
			NEncoding::CSimpleJSONDatabase m_StateDatabase;
			NEncoding::CSimpleJSONDatabase m_ConfigDatabase;
			TCActor<CDistributedActorTrustManager> m_TrustManager;
			TCActor<CActorDistributionManager> m_DistributionManager;

		private:
			friend struct CDistributedAppActor;
			
			CDistributedAppState(CDistributedAppActor_Settings const &_Settings);			
		};
		
		struct CDistributedAppActor : public CActor
		{
			struct CCommandLine : public ICCommandLine
			{
				CCommandLine(TCWeakActor<CDistributedAppActor> const &_Actor);
				
				TCContinuation<CDistributedAppCommandLineResults> f_RunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params) override;
				
			private:
				TCWeakActor<CDistributedAppActor> mp_Actor; 
			};
			
			CDistributedAppActor(CDistributedAppActor_Settings const &_Settings);
			~CDistributedAppActor();
			
			void f_Construct();

			TCContinuation<void> f_StartApp(NEncoding::CEJSON const &_Params); 
			TCContinuation<void> f_StopApp(); 
			
			TCContinuation<CDistributedAppCommandLineClient> f_GetCommandLineClient(); 

			TCContinuation<CDistributedAppCommandLineResults> f_RunCommandLine(NStr::CStr const &_FromHostID, NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
			
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddConnection(NStr::CStr const &_Ticket, bool _bIncludeFriendlyHostName); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GenerateTrustTicket(NStr::CStr const &_ForListen);

			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListTrustedHosts(bool _bIncludeFriendlyHostName);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveTrustedHost(NStr::CStr const &_HostID);
			
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GetHostID();
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GetConnetionStatus();
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListConnections(bool _bIncludeFriendlyHostName);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveConnection(NStr::CStr const &_URL);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddAdditionalConnection(NStr::CStr const &_URL, bool _bIncludeFriendlyHostName);
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddListen(NStr::CStr const &_URL); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveListen(NStr::CStr const &_URL); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_SetPrimaryListen(NStr::CStr const &_URL);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListListen();

			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListNamespaces(bool _bIncludeTrustedHosts);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_TrustHostForNamespace(NStr::CStr const &_Namespace, NStr::CStr const &_Host); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_UntrustHostForNamespace(NStr::CStr const &_Namespace, NStr::CStr const &_Host); 
			
			TCContinuation<void> f_Destroy();

		protected:
			virtual TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) = 0;
			virtual TCContinuation<void> fp_StopApp() = 0;
			virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine); 
			
			CDistributedAppState mp_State;
			
		private:
			TCContinuation<void> fp_Initialize();
			TCContinuation<void> fp_SetupListen();
			
			TCContinuation<void> fp_CreateCommandLineTrust();
			TCContinuation<void> fp_SetupCommandLineListen();
			TCContinuation<void> fp_SetupCommandLineTrust();
			
			TCContinuation<CDistributedAppCommandLineResults> fp_RunCommandLineAndLogError
				(
					NStr::CStr const &_Description
					, NFunction::TCFunction<TCContinuation<CDistributedAppCommandLineResults> ()> &&_fCommand
				)
			;
			
			TCContinuation<void> fp_PublishCommandLine();
			
			bool fp_HasCommandLineAccess(NStr::CStr const &_HostID);
			
			NHTTP::CURL fp_GetLocalAddress();

			TCActor<ICDistributedActorTrustManagerDatabase> mp_TrustManagerDatabase;
			TCActor<CSeparateThreadActor> mp_FileOperationsActor;
			TCDistributedActor<CCommandLine> mp_CommandLine;
			CDistributedActorPublication mp_CommandLinePublication;
			CDistributedActorTrustManager_Address mp_PrimaryListen;
			NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> mp_pCommandLineSpec;
			NPtr::TCUniquePointer<TCActorCallOnce<void>> mp_pInitOnce; 
			CDistributedAppActor_Settings mp_Settings;
		};

		void fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params);
		aint fg_RunApp
			(
				NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
				, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
				, bool _bStartApp = true 
			)
		;
	}
}
