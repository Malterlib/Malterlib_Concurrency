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
				)
				: m_AppName(_AppName)
				, m_bRequireListen(_bRequireListen)
				, m_ConfigDirectory(_ConfigDirectory)
				, m_bSeparateConcurrencyManager(_bSeparateConcurrencyManager)
				, m_KeySize(_KeySize)
			{
			}
			
			NStr::CStr m_AppName;
			NStr::CStr m_ConfigDirectory;
			bool m_bRequireListen = false;
			bool m_bSeparateConcurrencyManager = false;
			uint32 m_KeySize = 4096;
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

			TCContinuation<void> f_StartApp(); 
			
			TCContinuation<CDistributedAppCommandLineClient> f_GetCommandLineClient(); 

			TCContinuation<CDistributedAppCommandLineResults> f_RunCommandLine(NStr::CStr const &_FromHostID, NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);
			
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddConnection(NStr::CStr const &_Ticket); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GenerateTrustTicket(NStr::CStr const &_ForListen);

			/* TODO:
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GetHostID();
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_GetConnetionStatus();
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListConnections();
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveConnection(NStr::CStr const &_URL);
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddAdditionalConnection(NStr::CStr const &_URL);
			 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddListen(); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveListen(); 
			TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListListen();
			 */

			
			TCContinuation<void> f_Destroy();

		protected:
			virtual TCContinuation<void> fp_StartApp() pure;
			virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine); 
			
			NEncoding::CSimpleJSONDatabase mp_StateDatabase;
			NEncoding::CSimpleJSONDatabase mp_ConfigDatabase;
			TCActor<CDistributedActorTrustManager> mp_TrustManager;
			TCActor<CActorDistributionManager> mp_DistributionManager;
			
		private:
			TCContinuation<void> fp_Initialize();
			TCContinuation<void> fp_SetupListen();
			
			TCContinuation<void> fp_CreateCommandLineTrust();
			TCContinuation<void> fp_SetupCommandLineListen();
			TCContinuation<void> fp_SetupCommandLineTrust();
			
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
		
		aint fg_RunApp
			(
				NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
				, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
				, bool _bStartApp = true 
			)
		;
	}
}
