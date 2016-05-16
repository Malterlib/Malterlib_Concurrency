// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Internal.h"
#include <Mib/Stream/ByteVector>
#include <Mib/Encoding/Base64>
#include <Mib/Network/SSL>

namespace NMib
{
	namespace NConcurrency
 	{
		TCContinuation<void> CDistributedActorTrustManager::f_AddListen(CDistributedActorTrustManager_Address const &_Address)
		{
			auto &Internal = *mp_pInternal;
			TCContinuation<void> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation, _Address]()
					{
						auto &Internal = *mp_pInternal;
						
						CListenConfig ListenConfig;
						ListenConfig.m_Address = _Address;
						
						auto pOld = Internal.m_Listen.f_FindEqual(ListenConfig);
						if (pOld)
						{
							Continuation.f_SetException(DMibErrorInstance("Already listening to address"));
							return;
						}
						
						uint16 Port = _Address.m_URL.f_GetPortFromScheme();
						if (!Port)
						{
							Continuation.f_SetException(DMibErrorInstance("Port must be know for listen"));
							return;
						}
						
						auto fDoListen = [this, Continuation, Port, ListenConfig](CServerCertificate const *_pServerCert)
							{
								auto &Internal = *mp_pInternal;
								
								CActorDistributionListenSettings ListenSettings(Port);
								ListenSettings.m_PrivateKey = _pServerCert->m_PrivateKey;
								ListenSettings.m_CACertificate = Internal.m_BasicConfig.m_CACertificate;
								ListenSettings.m_PublicCertificate = _pServerCert->m_PublicCertificate;
								ListenSettings.m_bRetryOnListenFailure = true;
								
								Internal.m_ActorDistributionManager(&CActorDistributionManager::f_Listen, ListenSettings) 
									> [this, Continuation, ListenConfig](TCAsyncResult<CDistributedActorListenReference> &&_ListenRef)
									{
										auto &Internal = *mp_pInternal;
										
										if (!_ListenRef)
										{
											Continuation.f_SetException(fg_Format("Failed to listen: {}", _ListenRef.f_GetExceptionStr()));
											return;
										}
										auto &ListenState = Internal.m_Listen[ListenConfig];
										ListenState.m_ListenReference = fg_Move(*_ListenRef);
										
										Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddListenConfig, ListenConfig) 
											> [this, Continuation, ListenConfig](TCAsyncResult<void> &&_Result)
											{
												auto &Internal = *mp_pInternal;
												
												if (!_Result)
												{
													Internal.m_Listen.f_Remove(ListenConfig);
													
													Continuation.f_SetException
														(
															DMibErrorInstance
															(
																fg_Format("Failed to save new listen config to database: {}", _Result.f_GetExceptionStr())
															)
														)
													;
													return;
												}
												
												Continuation.f_SetResult();
											}
										;
									}
								;
							}
						;
						
						auto Host = _Address.m_URL.f_GetHost();
						
						auto *pServerCert = Internal.m_ServerCertificates.f_FindEqual(Host);
						if (pServerCert)
						{
							fDoListen(pServerCert);
							return;
						}				

						Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_GetNewCertificateSerial) > [this, Continuation, Host, fDoListen](TCAsyncResult<int32> &&_Serial)
							{
								auto &Internal = *mp_pInternal;
								
								if (!_Serial)
								{
									Continuation.f_SetException(DMibErrorInstance(fg_Format("Failed to get new certificate serial: {}", _Serial.f_GetExceptionStr())));
									return;
								}
								
								// Because of performance of generating a new key, run on separate actor
								fg_ConcurrentDispatch
									(
										[
											CaCertificate = Internal.m_BasicConfig.m_CACertificate
											, CaPrivateKey = Internal.m_BasicConfig.m_CAPrivateKey
											, KeySize = Internal.m_KeySize
											, Host
											, HostID = Internal.m_BasicConfig.m_HostID
											, Serial = *_Serial 
										]
										{
											NNet::CSSLContext::CCertificateOptions Options;
											Options.m_KeyLength = KeySize;
											Options.m_Subject = fg_Format("Malterlib Distributed Actors Listen - {}", Host).f_Left(64);
											Options.m_Hostnames.f_Insert(Host);
											auto &Extension = Options.m_Extensions["MalterlibHostID"].f_Insert();
											Extension.m_bCritical = false; 
											Extension.m_Value = HostID;
											
											NContainer::TCVector<uint8> CertificateRequest;
											ICDistributedActorTrustManagerDatabase::CServerCertificate ServerCert;
											try
											{
												NNet::CSSLContext::fs_GenerateClientCertificateRequest
													(
														Options
														, CertificateRequest
														, ServerCert.m_PrivateKey
													)
												;
											}
											catch (NException::CException const &_Exception)
											{
												DMibError(fg_Format("Failed to generate listen certificate request: {}", _Exception.f_GetErrorStr()));
											}
											
											try
											{
												NNet::CSSLContext::fs_SignClientCertificate
													(
														CaCertificate
														, CaPrivateKey
														, CertificateRequest
														, ServerCert.m_PublicCertificate
														, Serial
														, 10*365 
													)
												;
											}
											catch (NException::CException const &_Exception)
											{
												DMibError(fg_Format("Failed to generate sign certificate request: {}", _Exception.f_GetErrorStr()));
											}
											
											return ServerCert;
										}
									) 
									> [this, Continuation, Host, fDoListen](TCAsyncResult<ICDistributedActorTrustManagerDatabase::CServerCertificate> &&_ServerCert)
									{
										auto &Internal = *mp_pInternal;
								
										if (!_ServerCert)
										{
											Continuation.f_SetException(fg_Move(_ServerCert));
											return;
										}
										
										auto &ServerCert = *_ServerCert;
										Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_AddServerCertificate, Host, ServerCert) 
											> [this, Continuation, ServerCert, Host, fDoListen](TCAsyncResult<void> &&_Result)
											{
												if (!_Result)
												{
													Continuation.f_SetException
														(
															DMibErrorInstance
															(
																fg_Format("Failed to save new server certificate to database: {}", _Result.f_GetExceptionStr())
															)
														)
													;
													return;
												}

												auto &Internal = *mp_pInternal;
												
												Internal.m_ServerCertificates[Host] = ServerCert;								
												
												fDoListen(&ServerCert);
											}
										;
									}
								;
							}
						;
					}
				)
			;
			return Continuation;
		}
		
		TCContinuation<void> CDistributedActorTrustManager::f_RemoveListen(CDistributedActorTrustManager_Address const &_Address)
		{
			auto &Internal = *mp_pInternal; 
			TCContinuation<void> Continuation;
			Internal.f_RunAfterInit
				(
					Continuation
					, [this, Continuation, _Address]()
					{
						auto &Internal = *mp_pInternal; 
						ICDistributedActorTrustManagerDatabase::CListenConfig ListenConfig;
						ListenConfig.m_Address = _Address;
						
						Internal.m_Database(&ICDistributedActorTrustManagerDatabase::f_RemoveListenConfig, ListenConfig) 
							> [this, Continuation, ListenConfig](TCAsyncResult<void> &&_Result)
							{
								auto &Internal = *mp_pInternal;
								
								if (!_Result)
								{
									Continuation.f_SetException
										(
											DMibErrorInstance
											(
												fg_Format("Failed to remove listen config from database: {}", _Result.f_GetExceptionStr())
											)
										)
									;
									return;
								}
								
								auto *pListenConfig = Internal.m_Listen.f_FindEqual(ListenConfig);
								if (pListenConfig)
								{
									pListenConfig->m_ListenReference.f_Stop() > [this, ListenConfig, Continuation](TCAsyncResult<void> &&_Result)
										{
											if (!_Result)
											{
												Continuation.f_SetException
													(
														DMibErrorInstance
														(
															fg_Format("Failed to stop listen: {}", _Result.f_GetExceptionStr())
														)
													)
												;
												return;
											}
											auto &Internal = *mp_pInternal;
											Internal.m_Listen.f_Remove(ListenConfig);
											Continuation.f_SetResult();
										}
									;
								}
								else
									Continuation.f_SetResult();
							}
						;
					}
				)
			;
			return Continuation;
		}
		
		TCContinuation<NStr::CStr> CDistributedActorTrustManager::CInternal::f_ValidateClientAccess
			(
				NStr::CStr const &_HostID
				, NContainer::TCVector<NContainer::TCVector<uint8>> const &_CertificateChain
			)
		{
			if (_CertificateChain.f_IsEmpty())
				return DMibErrorInstance("Empty certificate chain");
			
			TCContinuation<NStr::CStr> Continuation;

			m_Database(&ICDistributedActorTrustManagerDatabase::f_GetClient, _HostID) > [this, Continuation, _HostID, _CertificateChain](TCAsyncResult<CClient> &&_Client)
				{
					if (!_Client)
					{
						Continuation.f_SetResult(fg_Format("Could not find client host: {}", _Client.f_GetExceptionStr()));
						return;
					}
					
					if (_CertificateChain.f_GetFirst() != _Client->m_PublicCertificate)
					{
						Continuation.f_SetResult("Access denied");
						return;
					}
								
					Continuation.f_SetResult("");
				}
			;
			
			return Continuation;
		}
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif
