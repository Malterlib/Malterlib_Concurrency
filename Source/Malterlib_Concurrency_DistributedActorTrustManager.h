// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include "Malterlib_Concurrency_DistributedActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Shared.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_Database.h"

namespace NMib
{
	namespace NConcurrency
	{
		class CDistributedActorTrustManager : public NConcurrency::CActor
		{
		public:
			struct CTrustTicket
			{
				enum
				{
					EVersion = 0x101
				};
				
				template <typename tf_CStream>
				void f_Feed(tf_CStream &_Stream) const;
				template <typename tf_CStream>
				void f_Consume(tf_CStream &_Stream);
				
				NStr::CStr f_ToStringTicket() const;
				static CTrustTicket fs_FromStringTicket(NStr::CStr const &_StringTicket);

				CDistributedActorTrustManager_Address m_ServerAddress;
				NContainer::TCVector<uint8> m_ServerPublicCert;
				NStr::CStr m_Token;
			};
			
			struct CAddressConnectionState
			{
				bool m_bConnected = false;
				NStr::CStr m_Error;
				
				CDistributedActorTrustManager_Address const &f_GetAddress() const
				{
					return NContainer::TCMap<CDistributedActorTrustManager_Address, CAddressConnectionState>::fs_GetKey(*this);
				}
				
			};
			
			struct CHostConnectionState
			{
				NContainer::TCMap<CDistributedActorTrustManager_Address, CAddressConnectionState> m_Addresses;
				NStr::CStr const &f_GetHostID() const
				{
					return NContainer::TCMap<NStr::CStr, CHostConnectionState>::fs_GetKey(*this);
				}
			};
			
			struct CConnectionState
			{
				NContainer::TCMap<NStr::CStr, CHostConnectionState> m_Hosts;
			};

			CDistributedActorTrustManager
				(
					NConcurrency::TCActor<ICDistributedActorTrustManagerDatabase> const &_Database
					, NFunction::TCFunction
					<
						NConcurrency::TCActor<NConcurrency::CActorDistributionManager> (NFunction::CThisTag &, NStr::CStr const &_HostID)
					> const &_fConstructManager = nullptr
					,  uint32 _KeySize = 4096
				)
			;
			
			~CDistributedActorTrustManager();
			
			void f_Construct() override;
			
			TCContinuation<void> f_Initialize(); 
			
			TCContinuation<void> f_AddListen(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveListen(CDistributedActorTrustManager_Address const &_Address);

			TCContinuation<CTrustTicket> f_GenerateConnectionTicket(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveClient(NStr::CStr const &_HostID);
			
			TCContinuation<void> f_AddClientConnection(CTrustTicket const &_TrustTicket, fp64 _Timeout);
			TCContinuation<void> f_AddAdditionalClientConnection(CDistributedActorTrustManager_Address const &_Address);
			TCContinuation<void> f_RemoveClientConnection(CDistributedActorTrustManager_Address const &_Address);

			TCContinuation<CConnectionState> f_GetConnectionState(bool _bWaitForAttepmts);
			
			TCContinuation<NStr::CStr> f_GetHostID() const; 
			TCContinuation<NConcurrency::TCActor<NConcurrency::CActorDistributionManager>> f_GetDistributionManager() const;
			
			// Handle renewal of certificates
			
		private:
			struct CInternal;
			NPtr::TCUniquePointer<CInternal> mp_pInternal;			
		};
	}
}

#ifndef DMibPNoShortCuts
	using namespace NMib::NConcurrency;
#endif

#include "Malterlib_Concurrency_DistributedActorTrustManager.hpp"
