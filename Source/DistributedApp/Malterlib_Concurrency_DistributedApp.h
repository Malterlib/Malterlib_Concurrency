// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Encoding/SimpleJSONDatabase>
#include <Mib/Concurrency/ActorCallOnce>

#include "Malterlib_Concurrency_DistributedApp_CommandLine.h"
#include "Malterlib_Concurrency_DistributedApp_CommandLineClient.h"

namespace NMib::NConcurrency
{
	enum EDistributedAppUpdateType
	{
		EDistributedAppUpdateType_Independent
		, EDistributedAppUpdateType_OneAtATime
		, EDistributedAppUpdateType_AllAtOnce
	};
	
	struct CDistributedAppActor_Settings
	{
		CDistributedAppActor_Settings() = default;
		CDistributedAppActor_Settings
			(
				NStr::CStr const &_AppName
				, bool _bRequireListen
				, NStr::CStr const &_ConfigDirectory = NFile::CFile::fs_GetProgramDirectory()
				, bool _bSeparateConcurrencyManager = false
				, NNet::CSSLKeySetting _KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting()
				, NStr::CStr const &_FriendlyName = NStr::CStr()
				, NStr::CStr const &_Enclave = fg_DistributedActorSuggestedEnclave()
				, EDistributedAppUpdateType _UpdateType = EDistributedAppUpdateType_Independent 
				, NStr::CStr const &_AuditCategory = {}
			)
		;
		NStr::CStr f_GetCompositeFriendlyName() const;
		NStr::CStr f_GetLocalSocketHostname(bool _bEnclaveSpecific) const;
		
		CDistributedAppActor_Settings &&f_ConfigDirectory(NStr::CStr const &_ConfigDirectory) &&;
		CDistributedAppActor_Settings &&f_FriendlyName(NStr::CStr const &_FriendlyName) &&;
		CDistributedAppActor_Settings &&f_Enclave(NStr::CStr const &_Enclave) &&;
		CDistributedAppActor_Settings &&f_AuditCategory(NStr::CStr const &_Category) &&;
		CDistributedAppActor_Settings &&f_SeparateConcurrencyManager(bool _bSeparateConcurrencyManager) &&;
		CDistributedAppActor_Settings &&f_KeySetting(NNet::CSSLKeySetting _KeySetting) &&;
		CDistributedAppActor_Settings &&f_UpdateType(EDistributedAppUpdateType _UpdateType) &&;
		
		NStr::CStr m_AppName;
		NStr::CStr m_ConfigDirectory;
		NStr::CStr m_FriendlyName;
		NStr::CStr m_AuditCategory;
		NStr::CStr m_Enclave;
		NNet::CSSLKeySetting m_KeySetting = CActorDistributionCryptographySettings::fs_DefaultKeySetting();
		NNet::ENetFlag m_ListenFlags = NNet::ENetFlag_None;
		EDistributedAppUpdateType m_UpdateType = EDistributedAppUpdateType_Independent;
		bool m_bRequireListen = false;
		bool m_bSeparateConcurrencyManager = false;
	private:
		NStr::CStr fp_GetLocalSocketPath(NStr::CStr const &_Prefix, bool _bEnclaveSpecific) const;
	};
	
	struct CDistributedAppActor;
	struct CDistributedAppInterfaceClient;
	struct CDistributedAppInterfaceServer;
	struct CDistributedAppInterfaceClientDistributedAppActor;
	
	struct CDistributedAppAuditor
	{
		CDistributedAppAuditor();
		~CDistributedAppAuditor();
		CDistributedAppAuditor(TCWeakActor<CDistributedAppActor> const &_AppActor, CCallingHostInfo const &_CallingHostInfo);
		NStr::CStr f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Audit(NLog::ESeverity _Severity, NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Critical(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Critical(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Error(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_Exception(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NStr::CStr const &_Message = {}, NStr::CStr const &_Category = {}) const;
		NException::CException f_AccessDenied(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		
		NStr::CStr f_Warning(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Warning(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NStr::CStr const &_Message, NStr::CStr const &_Category = {}) const;
		NStr::CStr f_Info(NContainer::TCVector<NStr::CStr> const &_Message, NStr::CStr const &_Category = {}) const;
		
		CCallingHostInfo const &f_HostInfo() const;
		
	private:
		CCallingHostInfo mp_CallingHostInfo;
		TCWeakActor<CDistributedAppActor> mp_AppActor;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithAppAuditor
	{
		TCContinuationWithAppAuditor(TCContinuation<t_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuation<t_CReturnValue> m_Continuation;
		CDistributedAppAuditor m_Auditor;
	};

	template <typename t_CReturnValue>
	struct TCContinuationWithErrorWithAppAuditor
	{
		TCContinuationWithErrorWithAppAuditor(TCContinuationWithError<t_CReturnValue> const &_Continuation, CDistributedAppAuditor const &_Auditor);

		template <typename tf_FResultHandler, TCEnableIfType<NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		template <typename tf_FResultHandler, TCEnableIfType<!NPrivate::TCAllAsyncResultsAreVoid<tf_FResultHandler>::mc_Value> * = nullptr>
		auto operator / (tf_FResultHandler &&_fResultHandler) const;

		TCContinuationWithError<t_CReturnValue> m_Continuation;
		CDistributedAppAuditor m_Auditor;
	};
	
	struct CDistributedAppState
	{
		NEncoding::CSimpleJSONDatabase m_StateDatabase;
		NEncoding::CSimpleJSONDatabase m_ConfigDatabase;
		TCActor<CDistributedActorTrustManager> m_TrustManager;
		TCActor<CActorDistributionManager> m_DistributionManager;
		TCDistributedActor<CDistributedAppInterfaceServer> m_AppInterfaceServer; 
		NHTTP::CURL m_LocalAddress;
		TCWeakActor<CDistributedAppActor> m_AppActor;
		NStr::CStr m_CommandLineHostID;
		bool m_bStoppingApp = false;

		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;
		
		CDistributedAppState() = delete;
		~CDistributedAppState();
		
		CDistributedAppState(CDistributedAppState const &);
		CDistributedAppState(CDistributedAppState &&);
		
		CDistributedAppState &operator = (CDistributedAppState const &);
		CDistributedAppState &operator = (CDistributedAppState &&);
		
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

		TCContinuation<CDistributedAppCommandLineResults> f_RunCommandLine(CCallingHostInfo const &_CallingHost, NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);

		uint32 f_CommandLine_RemoveAllTrust(bool _bConfirm);
		
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

		TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_ListHostPermissions(bool _bIncludeHosts);
		TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_AddHostPermission(NStr::CStr const &_HostID, NStr::CStr const &_Permission); 
		TCContinuation<CDistributedAppCommandLineResults> f_CommandLine_RemoveHostPermission(NStr::CStr const &_HostID, NStr::CStr const &_Permission); 
		
		TCContinuation<void> f_Destroy();
		
		void f_Audit(NLog::ESeverity _Severity, NStr::CStr const &_Message, NStr::CStr const &_Category, CCallingHostInfo const &_CallingHostInfo);
		
		CDistributedAppAuditor f_Auditor(CCallingHostInfo const &_CallingHostInfo = fg_GetCallingHostInfo()) const;

	protected:
		virtual TCContinuation<void> fp_StartApp(NEncoding::CEJSON const &_Params) = 0;
		virtual TCContinuation<void> fp_StopApp() = 0;
		virtual void fp_BuildCommandLine(CDistributedAppCommandLineSpecification &o_CommandLine); 
		virtual TCContinuation<CDistributedAppCommandLineResults> fp_PreRunCommandLine(NStr::CStr const &_Command, NEncoding::CEJSON const &_Params);

		virtual TCContinuation<void> fp_PreUpdate();
		
		CDistributedAppState mp_State;
		
	private:
		struct CDistributedAppInterfaceClientImplementation;
		
		TCContinuation<void> fp_Initialize();
		TCContinuation<void> fp_SetupListen();
		TCContinuation<void> fp_SetupAppServerInterface();
		TCContinuation<void> fp_SubscribeAppServerInterface();
		TCContinuation<CDistributedActorTrustManager::CTrustTicket> fp_GetTicketThroughStdIn(NStr::CStr const &_RequestMagic);
		
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
		
		NHTTP::CURL fp_GetLocalAddress() const;
		NStr::CStr fp_GetLocalHostname(bool _bEnclaveSpecific) const;
		NContainer::TCMap<NStr::CStr, NStr::CStr> fp_GetTranslateHostnames() const;

		TCActor<ICDistributedActorTrustManagerDatabase> mp_TrustManagerDatabase;
		TCActor<CSeparateThreadActor> mp_FileOperationsActor;
		TCDistributedActor<CCommandLine> mp_CommandLine;
		CDistributedActorPublication mp_CommandLinePublication;
		CDistributedActorTrustManager_Address mp_PrimaryListen;
		NPtr::TCSharedPointer<CDistributedAppCommandLineSpecification> mp_pCommandLineSpec;
		NPtr::TCUniquePointer<TCActorCallOnce<void>> mp_pInitOnce; 
		CDistributedAppActor_Settings mp_Settings;
		COnScopeExitShared mp_pStdInCleanup;
		
		TCTrustedActorSubscription<CDistributedAppInterfaceServer> mp_AppInteraceServerSubscription;
		TCDistributedActor<CDistributedAppInterfaceClient> mp_AppInterfaceClientImplementation;
		TCDistributedActor<CDistributedActorTrustManagerInterface> mp_AppInterfaceClientTrustProxy;
		CActorSubscription mp_AppInterfaceClientRegistrationSubscription;
		
		bool mp_bDelegateTrustToAppInterface = false;
	};

	bool fg_ApplyLoggingOption(NEncoding::CEJSON const &_Params);
	aint fg_RunApp
		(
			NFunction::TCFunction<TCActor<CDistributedAppActor> ()> const &_fActorFactory
			, NFunction::TCFunction<void (CDistributedAppCommandLineSpecification &o_CommandLine, CDistributedAppActor_Settings const &_Settings)> const &_fMutateCommandLine = nullptr
			, bool _bStartApp = true 
		)
	;
}

#include "Malterlib_Concurrency_DistributedApp.hpp"
