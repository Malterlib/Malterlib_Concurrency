// Copyright © 2015 Hansoft AB 
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/CommandLine/AnsiEncoding>
#include <Mib/Concurrency/DistributedApp>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Container/Vector>
#include <Mib/Cryptography/EncryptedStream>
#include <Mib/Cryptography/RandomData>
#include <Mib/Encoding/Base64>
#include <Mib/Cryptography/BoringSSL>

#include "Malterlib_Concurrency_DistributedActorTrustManager.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor.h"
#include "Malterlib_Concurrency_DistributedActorTrustManager_AuthenticationActor_U2F_HID.h"

extern "C"
{
#	ifdef final
#		undef final
#	endif
#	include <openssl/ssl.h>
#	include <openssl/ecdsa.h>
#	undef X509_NAME
#	include <openssl/x509v3.h>

#	ifndef final
#		define final
#	endif

#ifdef DPlatformFamily_OSX
	#include <Security/Security.h>
	#include <unistd.h>
#elif defined DPlatformFamily_Linux
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/ioctl.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif
};

#define CID_BROADCAST           0xffffffff			// Broadcast channel id
#define TYPE_INIT               0x80				// Initial frame identifier
#define U2FHID_PING         	(TYPE_INIT | 0x01)	// Echo data through local processor only
#define U2FHID_MSG          	(TYPE_INIT | 0x03)	// Send U2F message frame
#define U2FHID_INIT         	(TYPE_INIT | 0x06)	// Channel initialization
#define U2FHID_ERROR        	(TYPE_INIT | 0x3f)	// Error response
#define MAXDATASIZE 			16384
#define FIDO_USAGE_PAGE         0xf1d0				// FIDO alliance HID usage page
#define FIDO_USAGE_U2FHID       0x01				// U2FHID usage for top-level collection
#define U2F_REGISTER            0x01				// Registration command
#define U2F_AUTHENTICATE        0x02				// Authenticate/sign command

#define ERR_NONE                0x00	// No error
#define ERR_INVALID_CMD         0x01	// Invalid command
#define ERR_INVALID_PAR         0x02	// Invalid parameter
#define ERR_INVALID_LEN         0x03	// Invalid message length
#define ERR_INVALID_SEQ         0x04	// Invalid message sequencing
#define ERR_MSG_TIMEOUT         0x05	// Message has timed out
#define ERR_CHANNEL_BUSY        0x06	// Channel busy
#define ERR_LOCK_REQUIRED       0x0a	// Command requires channel lock
#define ERR_INVALID_CID         0x0b	// Command not allowed on this cid
#define ERR_OTHER               0x7f	// Other unspecified error

#define U2F_PUBLIC_KEY_LEN		65
#define U2F_COUNTER_LEN			4

namespace NMib::NConcurrency::NPrivate
{
	using namespace NMib;
	using namespace NStorage;
	using namespace NContainer;
	using namespace NConcurrency;
	using namespace NCryptography;
	using namespace NStr;
	using namespace NFunction;

	// An not so smart pointer that applies the SSL free functions
	template<typename t_CPointer, auto t_fDeleter>
	struct CSSLPointer
	{
		CSSLPointer(t_CPointer _pSSLHandle)
			: m_pSSLHandle(_pSSLHandle)
		{
		}

		CSSLPointer(CSSLPointer const &_Other) = delete;
		CSSLPointer &operator = (CSSLPointer const &_Other) = delete;

		CSSLPointer(CSSLPointer &&_Other)
			: m_pSSLHandle(_Other.m_pSSLHandle)
		{
			_Other.m_pSSLHandle = nullptr;
		}

		CSSLPointer &operator = (CSSLPointer &&_Other)
		{
			m_pSSLHandle = _Other.m_pSSLHandle;
			_Other.m_pSSLHandle = nullptr;
			return *this;
		}

		~CSSLPointer()
		{
			if (m_pSSLHandle)
				t_fDeleter(m_pSSLHandle);
		}

		t_CPointer operator -> () const
		{
			return m_pSSLHandle;
		}

		operator bool () const
		{
			return m_pSSLHandle != nullptr;
		}

		operator t_CPointer () const
		{
			return m_pSSLHandle;
		}

		t_CPointer f_StealPointer()
		{
			t_CPointer pTmp = m_pSSLHandle;
			m_pSSLHandle = nullptr;
			return pTmp;
		}

		t_CPointer m_pSSLHandle;
	};

	EC_KEY *fg_DecodeUserKey(CSecureByteVector const &_Data)
	{
		CSSLPointer<EC_GROUP *, EC_GROUP_free> pECGroup = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
		CSSLPointer<EC_KEY *, EC_KEY_free> pKey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		CSSLPointer<EC_POINT *, EC_POINT_free> pPoint = EC_POINT_new(pECGroup);

		EC_GROUP_set_point_conversion_form(pECGroup, POINT_CONVERSION_UNCOMPRESSED);

		ERR_clear_error();
		if (EC_POINT_oct2point(pECGroup, pPoint, _Data.f_GetArray(), U2F_PUBLIC_KEY_LEN, nullptr) == 0)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed decode user key (EC_POINT_oct2point)"));

		ERR_clear_error();
		if (EC_KEY_set_public_key(pKey, pPoint) == 0)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed decode user key (EC_KEY_set_public_key)"));

		return pKey.f_StealPointer();
	}

	CSecureByteVector fg_DumpUserKey(EC_KEY const *_pKey)
	{
		CSSLPointer<EC_GROUP *, EC_GROUP_free> pECGroup = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);

		if (!pECGroup)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed dump user key (EC_GROUP_new_by_curve_name)"));

		CSecureByteVector Buffer;
		EC_POINT const *pPoint = EC_KEY_get0_public_key(_pKey);
		ERR_clear_error();
		if (EC_POINT_point2oct(pECGroup, pPoint, POINT_CONVERSION_UNCOMPRESSED, Buffer.f_GetArray(U2F_PUBLIC_KEY_LEN), U2F_PUBLIC_KEY_LEN, nullptr) != U2F_PUBLIC_KEY_LEN)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed dump user key (EC_POINT_point2oct)"));

		// For some reason the libraries from Yubico produces U2F_PUBLIC_KEY_LEN + 1 bytes from its dump_user_key function.
		// The buffer also contains a zero termination, so we add one as well to get identical results.
		Buffer.f_InsertLast((uint8)0);
		return Buffer;
	}

	CStr fg_DumpX509Cert(X509 *_pCert)
	{
		ERR_clear_error();
		CSSLPointer<BIO *, BIO_free> pBio = BIO_new(BIO_s_mem());
		if (!pBio)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed dump x509 cert (BIO_new)"));

		ERR_clear_error();
		if (!PEM_write_bio_X509(pBio, _pCert))
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed dump x509 cert (PEM_write_bio_X509)"));

		char *pData = nullptr;
		auto Length = BIO_get_mem_data(pBio, &pData);
		if (Length < 0)
			DMibError(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed dump x509 cert (BIO_get_mem_data)"));

		return CStr(pData, Length);
	}

	// The unit of data passed to and from the U2F device
	struct CFrame
	{
		mint f_SetInitial(uint32 _ChannelID, uint8 _Command, uint16 _ByteCount, uint8 const *_pData, uint32 _Length)
		{
			_Length = fg_Min(_Length, uint32(CHumanInterfaceDevicesActor::EReportSize - 7));
			m_ChannelID = _ChannelID;
			m_InitialFrame.m_Command = _Command;
			m_InitialFrame.m_ByteCountHigh = _ByteCount >> 8;
			m_InitialFrame.m_ByteCountLow = _ByteCount;
			NMemory::fg_MemCopy(m_InitialFrame.m_Data, _pData, _Length);
			if (_Length < CHumanInterfaceDevicesActor::EReportSize - 7)
				NMemory::fg_SecureMemClear(m_InitialFrame.m_Data + _Length, (CHumanInterfaceDevicesActor::EReportSize - 7) - _Length);

			return _Length;
		}

		mint f_SetContinuation(uint32 _ChannelID, uint8 _SequenceNumber, uint8 const *_pData, uint32 _Length)
		{
			_Length = fg_Min(_Length, uint32(CHumanInterfaceDevicesActor::EReportSize - 5));
			m_ChannelID = _ChannelID;
			m_ContinuationFrame.m_SequenceNumber = _SequenceNumber;
			NMemory::fg_MemCopy(m_ContinuationFrame.m_Data, _pData, _Length);
			if (_Length < CHumanInterfaceDevicesActor::EReportSize - 5)
				NMemory::fg_SecureMemClear(m_ContinuationFrame.m_Data + _Length, (CHumanInterfaceDevicesActor::EReportSize - 5) - _Length);

			return _Length;
		}

		uint32 m_ChannelID;
		union
		{
			struct
			{
				uint8 m_Command;
				uint8 m_ByteCountHigh;
				uint8 m_ByteCountLow;
				uint8 m_Data[CHumanInterfaceDevicesActor::EReportSize - 7];
			} m_InitialFrame;
			struct
			{
				uint8 m_SequenceNumber;
				uint8 m_Data[CHumanInterfaceDevicesActor::EReportSize - 5];
			} m_ContinuationFrame;
		};
	};

	struct CU2FDevice : public CAllowUnsafeThis
	{
		CU2FDevice(TCActor<CHumanInterfaceDeviceActor> const &_Device, uint32 _ChannelID, CStr const &_Path)
			: m_Device(_Device)
			, m_ChannelID(_ChannelID)
			, m_Path(_Path)
		{
		}
		CU2FDevice(CU2FDevice &&) = default;
		CU2FDevice(CU2FDevice const &) = default;
		CU2FDevice &operator = (CU2FDevice &&) = default;
		CU2FDevice &operator = (CU2FDevice const &) = default;

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CU2FDevice> fs_Init(TCActor<CHumanInterfaceDeviceActor> _Device, CStr _Path)
		{
			static mint const c_InitNonceSize = 8;

			struct CInitResponse
			{
				uint8 m_Nonce[c_InitNonceSize];
				uint32 m_ChannelID;
				uint8 m_VersionInterface;
				uint8 m_VersionMajor;
				uint8 m_VersionMinor;
				uint8 m_VersionBuild;
				uint8 m_CapFlags;
			};

			CSecureByteVector Nonce;
			NCryptography::fg_GenerateRandomData(Nonce.f_GetArray(c_InitNonceSize), c_InitNonceSize);

			auto RawResponse = co_await fs_SendReceive(U2FHID_INIT, Nonce, CID_BROADCAST, _Device);

			if (RawResponse.f_GetLen() != (c_InitNonceSize + 4 + 5))
				co_return DMibErrorInstance("Invalid U2FHID_INIT response length");

			CInitResponse Resonse;
			NMemory::fg_MemCopy(&Resonse, RawResponse.f_GetArray(), c_InitNonceSize + 4 + 5);
			if (NMemory::fg_MemCmp(Resonse.m_Nonce, Nonce.f_GetArray(), c_InitNonceSize))
				co_return DMibErrorInstance("U2FHID_INIT response contains invalid nounce");

			co_return CU2FDevice{_Device, Resonse.m_ChannelID, _Path};
		}

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CFrame> fs_ReadFrame(TCActor<CHumanInterfaceDeviceActor> _Device)
		{
			constexpr static mint c_HIDTimeout = 2;
			constexpr static mint c_HIDMaxTimeout = 4096;
			mint Timeout = c_HIDTimeout;

			while (true)
			{
				auto Result = co_await _Device(&CHumanInterfaceDeviceActor::f_ReadTimeout, CHumanInterfaceDevicesActor::EReportSize, Timeout);

				if (!Result.f_IsEmpty())
				{
					CFrame Frame;
					if (Result.f_GetLen() != sizeof(Frame))
						co_return DMibErrorInstance("U2F transport error: Frame size wrong");

					NMemory::fg_MemCopy((uint8 *)&Frame, Result.f_GetArray(), sizeof(Frame));

					co_return fg_Move(Frame);
				}

				Timeout *= 2;

				if (Timeout > c_HIDMaxTimeout)
					break;
			}

			co_return DMibErrorInstance("U2F transport error: Timed out reading");
		}

		TCFuture<CSecureByteVector> f_SendReceive(uint8_t _Command, CSecureByteVector _Send) const
		{
			co_return co_await fs_SendReceive(_Command, _Send, m_ChannelID, m_Device);
		}

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CSecureByteVector> fs_SendReceive(uint8_t _Command, CSecureByteVector _Send, uint32 _ChannelID, TCActor<CHumanInterfaceDeviceActor> _Device)
		{
			{
				uint8 Sequence = 0;
				mint SendLength = _Send.f_GetLen();
				uint8 const *pSendData = _Send.f_GetArray();
				mint DataSent = 0;

				while (DataSent < SendLength)
				{
					CFrame Frame;

					if (DataSent == 0)
						DataSent += Frame.f_SetInitial(_ChannelID, _Command, SendLength, pSendData, SendLength);
					else
						DataSent += Frame.f_SetContinuation(_ChannelID, Sequence++, pSendData + DataSent, SendLength - DataSent);

					CSecureByteVector Data;
					Data.f_Insert((uint8 *)&Frame, sizeof(Frame));

					auto BytesWritten = co_await _Device(&CHumanInterfaceDeviceActor::f_Write, 0, fg_Move(Data));
					if (BytesWritten != sizeof(CFrame) + 1)
						co_return DMibErrorInstance("Wrong number of bytes written for U2F frame");
				}
			}

			{
				CSecureByteVector Receive;
				uint16 nReceived = 0;
				uint16 DataLength = 0;
				uint8 Sequence = 0;
				bool bFirstFrame = true;
				while (nReceived < DataLength || bFirstFrame)
				{
					auto Frame = co_await fs_ReadFrame(_Device);

					if (Frame.m_ChannelID != _ChannelID)
						co_return DMibErrorInstance("Invalid channel number reading from U2F device");

					if (bFirstFrame)
					{
						if (Frame.m_InitialFrame.m_Command == U2FHID_ERROR)
						{
							CStr Error = "Unknown error";
							uint16 DataLength = Frame.m_InitialFrame.m_ByteCountHigh << 8 | Frame.m_InitialFrame.m_ByteCountLow;
							if (DataLength == 1)
							{
								switch (Frame.m_InitialFrame.m_Data[0])
								{
								case ERR_INVALID_CMD: Error = "Invalid command"; break;
								case ERR_INVALID_PAR: Error = "Invalid parameter"; break;
								case ERR_INVALID_LEN: Error = "Invalid message length"; break;
								case ERR_INVALID_SEQ: Error = "Invalid message sequencing"; break;
								case ERR_MSG_TIMEOUT: Error = "Message has timed out"; break;
								case ERR_CHANNEL_BUSY: Error = "Channel busy"; break;
								case ERR_LOCK_REQUIRED: Error = "Command requires channel lock"; break;
								case ERR_INVALID_CID: Error = "Command not allowed on this cid"; break;
								}
							}

							co_return DMibErrorInstance("Error response reading from U2F device: {}"_f << Error);
						}

						if (Frame.m_InitialFrame.m_Command != _Command)
							co_return DMibErrorInstance("Invalid command reading from U2F device");

						DataLength = Frame.m_InitialFrame.m_ByteCountHigh << 8 | Frame.m_InitialFrame.m_ByteCountLow;
						auto Received = fg_Min(sizeof(Frame.m_InitialFrame.m_Data), mint(DataLength));
						Receive.f_Insert(Frame.m_InitialFrame.m_Data, Received);
						nReceived = Received;
						bFirstFrame = false;
					}
					else
					{
						if (Frame.m_ContinuationFrame.m_SequenceNumber != Sequence++)
							co_return DMibErrorInstance("Invalid sequence number when reading from U2F device");

						auto Received = fg_Min(mint(nReceived) + sizeof(Frame.m_ContinuationFrame.m_Data), mint(DataLength)) - mint(nReceived);
						Receive.f_Insert(Frame.m_ContinuationFrame.m_Data, Received);
						nReceived += Received;
					}

				}
				co_return fg_Move(Receive);
			}
		}

		TCFuture<CSecureByteVector> f_SendAPDU(uint8 _Command, CSecureByteVector _Data, uint8 _UserPresence) const
		{
			auto DataLength = _Data.f_GetLen();

			CSecureByteVector Buffer{0, _Command, _UserPresence, 0, 0, (uint8)(DataLength >> 8), (uint8)DataLength};
			Buffer.f_Insert(_Data);
			Buffer.f_Insert((uint8)0);
			Buffer.f_Insert((uint8)0);

			auto Response = co_await f_SendReceive(U2FHID_MSG, Buffer);

			if (Response.f_GetLen() < 2)
				co_return DMibErrorInstance("Response from U2F device too short");

			if (Response.f_GetLen() > MAXDATASIZE)
				co_return DMibErrorInstance("Response from U2F device too long");

			co_return fg_Move(Response);
		}

		TCActor<CHumanInterfaceDeviceActor> m_Device;
		CStr m_Path;
		uint32 m_ChannelID;
	};

	// Class for handling the low level communication with the U2F device, one CFrame at the time
	struct CU2FDevices : public CAllowUnsafeThis
	{
		struct CUsageResult
		{
			uint16 m_UsagePage;
			uint16 m_Usage;
		};

		CU2FDevices()
		{
		}

		~CU2FDevices()
		{
//			for (auto &Device : m_Devices)
//				Device.f_Close();
		}

#ifdef DPlatformFamily_Linux

#include <linux/hidraw.h>

		static uint32 fs_GetBytes(uint8 *_pReportDescriptor, size_t _Len, size_t _Bytes, size_t _Cur)
		{
			if (_Cur + _Bytes >= _Len)
				return 0;

			if (_Bytes == 0)
				return 0;

			if (_Bytes == 1)
				return _pReportDescriptor[_Cur + 1];

			if (_Bytes == 2)
				return (_pReportDescriptor[_Cur + 2] * 256 + _pReportDescriptor[_Cur + 1]);

			return 0;
		}

		static int fs_GetUsage(uint8 *_pReportDescriptor, size_t _Size, CUsageResult &_Result)
		{
			size_t i = 0;
			int KeySize;
			bool bFoundUsage = false;
			bool bFoundUsagePage = false;

			while (i < _Size)
			{
				int Key = _pReportDescriptor[i];
				int KeyCommand = Key & 0xfc;

				if ((Key & 0xf0) == 0xf0)
					return -1; // invalid data

				aint SizeCode = Key & 0x3;
				aint DataLength;
				switch (SizeCode)
				{
				case 0:
				case 1:
				case 2:
					DataLength = SizeCode;
					break;
				case 3:
					DataLength = 4;
					break;

				default:
					DMibNeverGetHere;
				};
				KeySize = 1;

				if (KeyCommand == 0x4)
				{
					_Result.m_UsagePage = fs_GetBytes(_pReportDescriptor, _Size, DataLength, i);
					bFoundUsagePage = 1;
				}
				if (KeyCommand == 0x8)
				{
					_Result.m_Usage = fs_GetBytes(_pReportDescriptor, _Size, DataLength, i);
					bFoundUsage = 1;
				}

				if (bFoundUsagePage && bFoundUsage)
					return 0;		/* success */

				i += DataLength + KeySize;
			}
			return -1;
		}

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CUsageResult> fs_GetUsages(TCSharedPointer<CU2FDevices> const &_pDevices, CHumanInterfaceDevicesActor::CDeviceInfo const &_DeviceInfo)
		{
			CUsageResult Result;
			int DescriptorSize;
			struct hidraw_report_descriptor ReportDescriptor;
			int Handle = open(_DeviceInfo.m_Path, O_RDWR);
			if (Handle > 0)
			{
				memset (&ReportDescriptor, 0, sizeof (ReportDescriptor));
				if (ioctl(Handle, HIDIOCGRDESCSIZE, &DescriptorSize) >= 0)
				{
					ReportDescriptor.size = DescriptorSize;
					if (ioctl(Handle, HIDIOCGRDESC, &ReportDescriptor) >= 0 && fs_GetUsage(ReportDescriptor.value, ReportDescriptor.size, Result) >= 0)
						co_return Result;
				}
				close(Handle);
			}

			co_return {};
		}
#else
		static DMibSuppressUndefinedSanitizerLinux TCFuture<CUsageResult> fs_GetUsages(TCSharedPointer<CU2FDevices> const &_pDevices, CHumanInterfaceDevicesActor::CDeviceInfo const &_DeviceInfo)
		{
			co_return CUsageResult{_DeviceInfo.m_UsagePage, _DeviceInfo.m_Usage};
		}
#endif

		static DMibSuppressUndefinedSanitizerLinux TCFuture<TCSharedPointer<CU2FDevices>> fs_DiscoverDevices()
		{
			TCSharedPointer<CU2FDevices> pDevices = fg_Construct();

			auto DeviceInfos = co_await pDevices->m_HID(&CHumanInterfaceDevicesActor::f_Enumerate, 0, 0);

			TCActorResultVector<CUsageResult> UsageResults;

			for (auto const &DeviceInfo : DeviceInfos)
				fs_GetUsages(pDevices, DeviceInfo) > UsageResults.f_AddResult();

			TCActorResultVector<TCActor<CHumanInterfaceDeviceActor>> OpenDeviceResults;
			{
				auto Usages = co_await UsageResults.f_GetResults();
				auto iDeviceInfo = DeviceInfos.f_GetIterator();

				TCVector<CHumanInterfaceDevicesActor::CDeviceInfo> NewDeviceInfos;

				for (auto &UsageResult : Usages)
				{
					auto &DeviceInfo = *iDeviceInfo;
					++iDeviceInfo;

					if (!UsageResult)
					{
						DMibLogWithCategory(Malterlib/Concurrency/U2F, Error, "Failed to get usages for U2F USB device '{}': {}", DeviceInfo.m_Path, UsageResult.f_GetExceptionStr());
						continue;
					}

					auto &Usage = *UsageResult;
					if (Usage.m_UsagePage == FIDO_USAGE_PAGE && Usage.m_Usage == FIDO_USAGE_U2FHID)
					{
						pDevices->m_HID(&CHumanInterfaceDevicesActor::f_OpenPath, DeviceInfo.m_Path) > OpenDeviceResults.f_AddResult();
						NewDeviceInfos.f_Insert(fg_Move(DeviceInfo));
					}
				}

				DeviceInfos = fg_Move(NewDeviceInfos);
			}


			TCActorResultVector<CU2FDevice> DeviceInits;
			{
				auto OpenedDevices = co_await OpenDeviceResults.f_GetResults();
				auto iDeviceInfo = DeviceInfos.f_GetIterator();

				TCVector<CHumanInterfaceDevicesActor::CDeviceInfo> NewDeviceInfos;

				for (auto &Device : OpenedDevices)
				{
					auto &DeviceInfo = *iDeviceInfo;
					++iDeviceInfo;

					if (!Device)
					{
						DMibLogWithCategory(Malterlib/Concurrency/U2F, Error, "Failed to open U2F USB device '{}': {}", DeviceInfo.m_Path, Device.f_GetExceptionStr());
						continue;
					}

					CU2FDevice::fs_Init(*Device, DeviceInfo.m_Path) > DeviceInits.f_AddResult();

					NewDeviceInfos.f_Insert(fg_Move(DeviceInfo));
				}

				DeviceInfos = fg_Move(NewDeviceInfos);
			}

			{
				auto InitializedDevices = co_await DeviceInits.f_GetResults();
				auto iDeviceInfo = DeviceInfos.f_GetIterator();
				for (auto &Device : InitializedDevices)
				{
					[[maybe_unused]] auto &DeviceInfo = *iDeviceInfo;
					++iDeviceInfo;

					if (!Device)
					{
						DMibLogWithCategory
							(
								Malterlib/Concurrency/U2F
								, Error
								, "Failed to init U2F USB device '{}': {}"
								, DeviceInfo.m_Path
								, Device.f_GetExceptionStr()
							)
						;
						continue;
					}

					pDevices->m_Devices.f_Insert(fg_Move(*Device));
				}
			}

			co_return fg_Move(pDevices);
		}

		TCActor<CHumanInterfaceDevicesActor> m_HID = fg_HumanInterfaceDevicesActor();
		TCVector<CU2FDevice> m_Devices;
	};

	// Class for handling the high level U2F communication
	struct CU2FContext : public CAllowUnsafeThis
	{
		struct CRegistrationResult
		{
			CSecureByteVector m_KeyHandle;
			NContainer::CSecureByteVector m_PublicKey;
			CStr m_AttestationCertificatePEM;
			CStr m_AppID;
		};

		struct CAuthenticationResponse
		{
			CSecureByteVector m_Signature;
			CStr m_FactorID;
			CStr m_FactorName;
		};

		struct CAuthenticationResult
		{
			bool m_bVerified = false;
			uint32 m_Counter = 0;
			uint8 m_UserPresence = 0;
		};

		struct CSendAPDUResult
		{
			CSecureByteVector m_Data;
			uint32 m_Index;
		};

		CU2FContext()
		{
			m_AppID = NCryptography::fg_RandomID();
			NCryptography::fg_GenerateRandomData(m_SignatureBytes.f_GetArray(32), 32);
		}

		CU2FContext(CAuthenticationData const &_AuthenticationData, CSecureByteVector const &_SignatureBytes, CStr const &_ID)
			: m_SignatureBytes(_SignatureBytes)
			, m_FactorID(_ID)
			, m_FactorName(_AuthenticationData.m_Name)
		{

			if (auto *pValue = _AuthenticationData.m_PrivateData.f_FindEqual("KeyHandle"))
			{
				if (pValue->f_IsBinary())
					m_KeyHandle = pValue->f_Binary().f_ToSecure();
			}
			if (auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("AppID"))
			{
				if (pValue->f_IsString())
					m_AppID = pValue->f_String();
			}
			if (auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("PublicKey"))
			{
				if (pValue->f_IsBinary())
					m_PublicKey = pValue->f_Binary().f_ToSecure();
			}
		}

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CSendAPDUResult> fs_SendAPDUs(uint32 _Command, TCVector<CSecureByteVector> _Data, TCFunction<void ()> _fPrompt)
		{
			auto Data = _Data;
			auto fPrompt = _fPrompt;

			auto pU2FDevices = co_await CU2FDevices::fs_DiscoverDevices();

			auto &U2FDevices = *pU2FDevices;

			if (U2FDevices.m_Devices.f_IsEmpty())
				co_return DMibErrorInstance("No U2F devices found");

			TCVector<TCSet<uint32>> SkipDevice;
			mint nIterations = 16;
			bool bSkippedAll = true;
			bool bDidPrompt = false;

			SkipDevice.f_SetLen(_Data.f_GetLen());

			while (true)
			{
				if (--nIterations == 0)
					co_return DMibErrorInstance("Timed out waiting for U2F button press");

				bSkippedAll = true;

				for (mint iData = 0; iData < Data.f_GetLen(); ++iData)
				{
					for (auto &Device : U2FDevices.m_Devices)
					{
						if (SkipDevice[iData].f_FindEqual(Device.m_ChannelID))
							continue;

						bSkippedAll = false;

						auto ResponseResult = co_await Device.f_SendAPDU(_Command, Data[iData], 3).f_Wrap();

						if (!ResponseResult)
						{
							DMibLogWithCategory(Malterlib/Concurrency/U2F, Error, "Failed to send APDU to U2F USB device: {}", ResponseResult.f_GetExceptionStr());

							SkipDevice[iData][Device.m_ChannelID];
							continue;
						}

						auto &Response = *ResponseResult;

						if (Response.f_GetLen() < 2)
							SkipDevice[iData][Device.m_ChannelID];
						else
						{
							mint ResponseLen = Response.f_GetLen();
							uint16 StatusCode = uint16(Response[ResponseLen - 2] << 8) | uint16(Response[ResponseLen - 1]);

							switch (StatusCode)
							{
							case 0x9000: // SW_NO_ERROR The command completed successfully without error.
								{
									Response.f_SetLen(ResponseLen - 2);

									CSendAPDUResult Result;
									Result.m_Data = fg_Move(Response);
									Result.m_Index = iData;

									co_return fg_Move(Result);
									break;
								}
							case 0x6985: // SW_CONDITIONS_NOT_SATISFIED The request was rejected due to test-of-user-presence being required.
								{
									if (!bDidPrompt)
									{
										bDidPrompt = true;
										fPrompt();
									}
									break;
								}
							case 0x6A80: // SW_WRONG_DATA The request was rejected due to an invalid key handle.
							case 0x6700: // SW_WRONG_LENGTH The length of the request was invalid.
							case 0x6E00: // SW_CLA_NOT_SUPPORTED The Class byte of the request is not supported.
							case 0x6D00: // SW_INS_NOT_SUPPORTED The Instruction of the request is not supported.
							default:
								{
									SkipDevice[iData][Device.m_ChannelID];
									break;
								}
							}
						}
					}
				}

				if (bSkippedAll)
					co_return DMibErrorInstance("No valid U2F devices");

				co_await fg_Timeout(0.5);
			}
		}

		TCFuture<CRegistrationResult> f_VerifyRegistrationResponse
			(
			 	CSecureByteVector _RegistrationData
			 	, CHashDigest_SHA256 _ChallengeDigest
			 	, CHashDigest_SHA256 _AppDigest
			 	, CStr _AppID
			) const
		{
			try
			{
				// Verify that the registration data is genuine

				/*
					Layout of data
					+-------------------------------------------------------------------+
					| 1 |     65    | 1 |    L    |    implied               | 64       |
					+-------------------------------------------------------------------+
					0x05
					public key
					key handle length
					key handle
					attestation cert
					signature
				*/

				if (_RegistrationData.f_GetLen() < 1 + 65 + 1 + 64)
				{
					co_return DMibErrorInstance("Registration data too short");
				}

				NStream::CBinaryStreamMemoryPtr<NStream::CBinaryStreamBigEndian> Stream;
				Stream.f_OpenRead(_RegistrationData);

				{
					uint8 Reserverd;
					Stream >> Reserverd;

					if (Reserverd != 0x05)
						co_return DMibErrorInstance("Reserved byte mismatch");
				}

				CSecureByteVector PublicKey;
				Stream.f_ConsumeBytes(PublicKey.f_GetArray(U2F_PUBLIC_KEY_LEN), U2F_PUBLIC_KEY_LEN);

				uint8 KeyHandleLen;
				Stream >> KeyHandleLen;

				CSecureByteVector KeyHandle;
				Stream.f_ConsumeBytes(KeyHandle.f_GetArray(KeyHandleLen), KeyHandleLen);

				CSecureByteVector AttestationCertData;
				{
					uint8 Reserved0;
					uint8 Reserved1;

					Stream >> Reserved0;
					Stream >> Reserved1;

					// Skip over offset and offset+1 (0x30, 0x82 respecitvely)
					// Length is big-endian encoded in offset+3 and offset+4
					if (Reserved0 != 0x30 || Reserved1 != 0x82)
						co_return DMibErrorInstance("Reserved byte mismatch");

					uint16 AttestationCertificateLen;
					Stream >> AttestationCertificateLen;
					Stream.f_AddPosition(-4);
					AttestationCertificateLen += 4;
					Stream.f_ConsumeBytes(AttestationCertData.f_GetArray(AttestationCertificateLen), AttestationCertificateLen);
				}

				CSSLPointer<X509 *, X509_free> pCertificate{nullptr};
				{
					uint8 const *pTempData = AttestationCertData.f_GetArray();
					ERR_clear_error();
					pCertificate = d2i_X509(nullptr, &pTempData, AttestationCertData.f_GetLen());
					if (!pCertificate)
						co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify registration response (d2i_X509)"));
				}

				CSSLPointer<ECDSA_SIG *, ECDSA_SIG_free> pSig{nullptr};
				{
					size_t SignatureLen = Stream.f_GetLength() - Stream.f_GetPosition();
					CSecureByteVector SignatureBuffer;
					Stream.f_ConsumeBytes(SignatureBuffer.f_GetArray(SignatureLen), SignatureLen);

					uint8 const *pTempData = SignatureBuffer.f_GetArray();
					ERR_clear_error();
					pSig = d2i_ECDSA_SIG(nullptr, &pTempData, SignatureBuffer.f_GetLen());
					if (!pSig)
						co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify registration response (d2i_ECDSA_SIG)"));
				}

				CSSLPointer<EVP_PKEY *, EVP_PKEY_free> pKey = X509_get_pubkey(pCertificate);
				if (!pKey)
					co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify registration response (X509_get_pubkey)"));

				CSSLPointer<EC_KEY *, EC_KEY_free> pECKey = EVP_PKEY_get1_EC_KEY(pKey);
				if (!pECKey)
					co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify registration response (EVP_PKEY_get1_EC_KEY)"));

				CSSLPointer<EC_GROUP *, EC_GROUP_free> pECGroup = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
				EC_KEY_set_asn1_flag(pECKey, OPENSSL_EC_NAMED_CURVE);
				EC_KEY_set_group(pECKey, pECGroup);

				CHash_SHA256 Hash;
				{
					uint8 Init = 0;
					Hash.f_AddData(&Init, 1);
					Hash.f_AddData(_AppDigest.f_GetData(), _AppDigest.fs_GetSize());
					Hash.f_AddData(_ChallengeDigest.f_GetData(), _ChallengeDigest.fs_GetSize());
					Hash.f_AddData(KeyHandle.f_GetArray(), KeyHandle.f_GetLen());
					Hash.f_AddData(PublicKey.f_GetArray(), PublicKey.f_GetLen());
				}
				auto Digest = Hash.f_GetDigest();

				ERR_clear_error();
				int VerifyResult = ECDSA_do_verify(Digest.f_GetData(), Digest.fs_GetSize(), pSig, pECKey);
				if (VerifyResult != 1)
				{
					if (VerifyResult == -1)
						co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify registration response (ECDSA_do_verify)"));
					else
						co_return DMibErrorInstance("Invalid signature");
				}

				CSSLPointer<EC_KEY *, EC_KEY_free> pUserPublicKey = fg_DecodeUserKey(PublicKey);
				co_return CRegistrationResult{KeyHandle, fg_DumpUserKey(pUserPublicKey), fg_DumpX509Cert(pCertificate), _AppID};
			}
			catch (NException::CException const &_Exception)
			{
				co_return _Exception.f_ExceptionPointer();
			}
		}

		TCFuture<CRegistrationResult> f_Register(TCFunction<void ()> _fPrompt)
		{
			CSecureByteVector Data;

			auto ChallengeDigest = CHash_SHA256::fs_DigestFromData(m_SignatureBytes);
			Data.f_Insert(ChallengeDigest.f_GetData(), ChallengeDigest.fs_GetSize());

			auto AppDigest = CHash_SHA256::fs_DigestFromData(m_AppID.f_GetStr(), m_AppID.f_GetLen());
			Data.f_Insert(AppDigest.f_GetData(), AppDigest.fs_GetSize());

			auto RegistrationData = co_await fs_SendAPDUs(U2F_REGISTER, {Data}, _fPrompt);
			DMibRequire(RegistrationData.m_Index == 0);

			co_return co_await f_VerifyRegistrationResponse(RegistrationData.m_Data, ChallengeDigest, AppDigest, m_AppID);
		}

		static DMibSuppressUndefinedSanitizerLinux TCFuture<CAuthenticationResponse> fs_Authenticate(TCVector<CU2FContext> _U2FContexts, TCFunction<void ()> _fPrompt)
		{
			struct CFactorValues
			{
				CStr m_AppID;
				CStr m_FactorID;
				CStr m_FactorName;
			};

			TCVector<CFactorValues> FactorValues;
			// Build the packets to send to the key
			TCVector<CSecureByteVector> DataUnits;
			for (auto const &Context : _U2FContexts)
			{
				CSecureByteVector Data;
				FactorValues.f_InsertLast({Context.m_AppID, Context.m_FactorID, Context.m_FactorName});

				auto Digest = CHash_SHA256::fs_DigestFromData(Context.m_SignatureBytes);
				Data.f_Insert(Digest.f_GetData(), Digest.fs_GetSize());

				Digest = CHash_SHA256::fs_DigestFromData(Context.m_AppID.f_GetStr(), Context.m_AppID.f_GetLen());
				Data.f_Insert(Digest.f_GetData(), Digest.fs_GetSize());

				Data.f_Insert(Context.m_KeyHandle.f_GetLen());
				Data.f_Insert(Context.m_KeyHandle);

				DataUnits.f_InsertLast(fg_Move(Data));
			}

			auto AuthenticationData = co_await fs_SendAPDUs(U2F_AUTHENTICATE, DataUnits, _fPrompt);

			if (!FactorValues.f_IsPosValid(AuthenticationData.m_Index))
				co_return DMibErrorInstance("Invalid index in U2F authentication USB device communication (fs_SendAPDUs)");

			auto const &Values = FactorValues[AuthenticationData.m_Index];

			co_return {fg_Move(AuthenticationData.m_Data), Values.m_FactorID, Values.m_FactorName};
		}

		TCFuture<CAuthenticationResult> f_VerifyAuthenticationResponse(CAuthenticationResponse const &_Response) const
		{
			CAuthenticationResult Result;

			// Parse the signature data

			/*
				Layout of data
				+-----------------------------------+
				| 1 |     4     |      implied      |
				+-----------------------------------+
				user presence
				counter
				signature
			*/

			if (_Response.m_Signature.f_GetLen() < 1 + U2F_COUNTER_LEN)
				co_return DMibErrorInstance("Length mismatch");

			auto *pData = _Response.m_Signature.f_GetArray();
			Result.m_UserPresence = *pData++;
			NMemory::fg_MemCopy((uint8 *)&Result.m_Counter, pData, U2F_COUNTER_LEN);
			pData += U2F_COUNTER_LEN;

			ERR_clear_error();
			CSSLPointer<ECDSA_SIG *, ECDSA_SIG_free> pSignature = d2i_ECDSA_SIG(nullptr, (uint8 const **)&pData, _Response.m_Signature.f_GetLen() - U2F_COUNTER_LEN - 1);
			if (!pSignature)
				co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify authentication response (d2i_ECDSA_SIG)"));

			CHash_SHA256 Hash;
			auto AppDigest = CHash_SHA256::fs_DigestFromData(m_AppID.f_GetStr(), m_AppID.f_GetLen());
			auto ClientDigest = CHash_SHA256::fs_DigestFromData(m_SignatureBytes);

			Hash.f_AddData(AppDigest.f_GetData(), AppDigest.fs_GetSize());
			Hash.f_AddData((uint8 *)&Result.m_UserPresence, 1);
			Hash.f_AddData((uint8 *)&Result.m_Counter, U2F_COUNTER_LEN);
			Hash.f_AddData(ClientDigest.f_GetData(), ClientDigest.fs_GetSize());
			auto Digest = Hash.f_GetDigest();

			CSSLPointer<EC_KEY *, EC_KEY_free> Key = fg_DecodeUserKey(m_PublicKey);
			ERR_clear_error();
			auto Verified = ECDSA_do_verify(Digest.f_GetData(), Digest.fs_GetSize(), pSignature, Key);
			if (Verified != 1)
			{
				if (Verified == -1)
					co_return DMibErrorInstance(NCryptography::NBoringSSL::fg_GetExceptionStr("Failed to verify authentication response (ECDSA_do_verify)"));
				else
					co_return DMibErrorInstance("Invalid signature");
			}
			// Change endianess
			Result.m_Counter = fg_ByteSwapBE(Result.m_Counter);
			Result.m_bVerified = true;

			co_return fg_Move(Result);
		}

		CSecureByteVector m_SignatureBytes;
		CSecureByteVector m_KeyHandle;
		CSecureByteVector m_PublicKey;
		CStr m_AppID;
		CStr m_FactorID;
		CStr m_FactorName;
	};
}

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NContainer;
	using namespace NStorage;

	class CDistributedActorTrustManagerAuthenticationActorU2F : public ICDistributedActorTrustManagerAuthenticationActor
	{
	public:
		CDistributedActorTrustManagerAuthenticationActorU2F(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager);
		virtual ~CDistributedActorTrustManagerAuthenticationActorU2F();

		TCFuture<CAuthenticationData> f_RegisterFactor(CStr const &_UserID, TCSharedPointer<CCommandLineControl> const &_pCommandLine) override;
		TCFuture<ICDistributedActorAuthenticationHandler::CResponse> f_SignAuthenticationRequest
			(
				TCSharedPointer<CCommandLineControl> const &_pCommandLine
				, CStr const &_Description
				, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
			 	, TCMap<CStr, CAuthenticationData> const &_Factors
			) override
		;
		TCFuture<CVerifyAuthenticationReturn> f_VerifyAuthenticationResponse
			(
			 	ICDistributedActorAuthenticationHandler::CResponse const &_Response
			 	, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			 	, CAuthenticationData const &_AuthenticationData
			) override
		;

	private:
		void fp_CreateProcessActor();

		TCWeakActor<CDistributedActorTrustManager> const mp_TrustManager;
		TCActor<CSeparateThreadActor> mp_ProcessActor;
	};

	CDistributedActorTrustManagerAuthenticationActorU2F::CDistributedActorTrustManagerAuthenticationActorU2F(TCWeakActor<CDistributedActorTrustManager> const &_TrustManager)
		: mp_TrustManager(_TrustManager)
	{
	}

	CDistributedActorTrustManagerAuthenticationActorU2F::~CDistributedActorTrustManagerAuthenticationActorU2F() = default;

	void CDistributedActorTrustManagerAuthenticationActorU2F::fp_CreateProcessActor()
	{
		if (!mp_ProcessActor)
			mp_ProcessActor = fg_Construct(fg_Construct(), "U2F");
	}

	TCFuture<CAuthenticationData> CDistributedActorTrustManagerAuthenticationActorU2F::f_RegisterFactor
		(
			CStr const &_UserID
			, TCSharedPointer<CCommandLineControl> const &_pCommandLine
		)
	{
		fp_CreateProcessActor();

		co_return co_await
			(
				g_Dispatch(mp_ProcessActor) / [=]() -> TCFuture<CAuthenticationData>
				{
					NPrivate::CU2FContext U2FContext;
					auto Result = co_await U2FContext.f_Register
						(
							[=]
							{
								auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

								*_pCommandLine %= "Adding U2F factor to user. {}Please press the button on your U2F device.{}\n"_f
									<< AnsiEncoding.f_Prompt()
									<< AnsiEncoding.f_Default()
								;
							}
						)
					;
					CAuthenticationData AuthenticationData;
					AuthenticationData.m_Category = EAuthenticationFactorCategory_Possession;
					AuthenticationData.m_Name = "U2F";
					AuthenticationData.m_PublicData["AttestationCertificate"] = Result.m_AttestationCertificatePEM;
					AuthenticationData.m_PublicData["PublicKey"] = Result.m_PublicKey.f_ToInsecure();
					AuthenticationData.m_PublicData["AppID"] = Result.m_AppID;
					AuthenticationData.m_PrivateData["KeyHandle"] = Result.m_KeyHandle.f_ToInsecure();
					co_return fg_Move(AuthenticationData);
				}
			)
		;
	}

	TCFuture<ICDistributedActorAuthenticationHandler::CResponse> CDistributedActorTrustManagerAuthenticationActorU2F::f_SignAuthenticationRequest
		(
			TCSharedPointer<CCommandLineControl> const &_pCommandLine
			, CStr const &_Description
			, ICDistributedActorAuthenticationHandler::CSignedProperties const &_SignedProperties
			, TCMap<CStr, CAuthenticationData> const &_Factors
		)
	{
		fp_CreateProcessActor();

		co_return co_await
			(
				g_Dispatch(mp_ProcessActor) / [=]() -> TCFuture<ICDistributedActorAuthenticationHandler::CResponse>
				{
					TCActorResultMap<TCTuple<CStr, CStr>, ICDistributedActorAuthenticationHandler::CResponse> AuthenticationResults;

					auto SignatureBytes = _SignedProperties.f_GetSignatureBytes();

					// This is handled much more smoothly for the password factor. There we ask for a password and can check all registered factors in parallel and see if the password
					// can decrypt the private key.
					//
					// If there are multiple keys plugged in to the computer they will all be tested "in parallel", SendAPDU will query all available keys at the same time, but we will
					// only get a valid reply from the key matching the keyhandle in the factor. If we have multiple registered factors we will have to test them one at a time, if we
					// don't start with the correct one we will have to wait until the SendAPDU query times out :(
					TCVector<NPrivate::CU2FContext> U2FContexts;

					for (auto const &Factor : _Factors)
						U2FContexts.f_Insert(NPrivate::CU2FContext(Factor, SignatureBytes, _Factors.fs_GetKey(Factor)));

					auto Response = co_await NPrivate::CU2FContext::fs_Authenticate
						(
							U2FContexts
							, [=]
							{
								auto AnsiEncoding = _pCommandLine->f_AnsiEncoding();

								*_pCommandLine %= "{}Please press the button on your U2F device.{}\n"_f
									<< AnsiEncoding.f_Prompt()
									<< AnsiEncoding.f_Default()
								;
							}
						)
					;

					ICDistributedActorAuthenticationHandler::CResponse Result;

					Result.m_FactorID = Response.m_FactorID;
					Result.m_FactorName = Response.m_FactorName;

					Result.m_SignedProperties = _SignedProperties;
					Result.m_Signature = Response.m_Signature;

					co_return fg_Move(Result);
				}
			)
		;
	};

	auto CDistributedActorTrustManagerAuthenticationActorU2F::f_VerifyAuthenticationResponse
		(
			ICDistributedActorAuthenticationHandler::CResponse const &_Response
			, ICDistributedActorAuthenticationHandler::CChallenge const &_Challenge
			, CAuthenticationData const &_AuthenticationData
		)
		-> TCFuture<CVerifyAuthenticationReturn>
	{
		fp_CreateProcessActor();

		co_return co_await
			(
				g_Dispatch(mp_ProcessActor) / [=]() -> TCFuture<CVerifyAuthenticationReturn>
				{
					auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("PublicKey");
					if (!pValue || !pValue->f_IsBinary())
						co_return CVerifyAuthenticationReturn{};

					NPrivate::CU2FContext::CAuthenticationResponse AuthenticationResponse;
					AuthenticationResponse.m_Signature = _Response.m_Signature;
					auto SignatureBytes = _Response.m_SignedProperties.f_GetSignatureBytes();

					NPrivate::CU2FContext U2FContext(_AuthenticationData, SignatureBytes, "");
					auto Result = co_await U2FContext.f_VerifyAuthenticationResponse(AuthenticationResponse);

					// Should we care about the counter?
					CVerifyAuthenticationReturn Return;
					Return.m_bVerified = Result.m_bVerified;

					if (auto *pValue = _AuthenticationData.m_PublicData.f_FindEqual("Counter"); pValue && pValue->f_IsInteger())
					{
						int64 NewCounter = Result.m_Counter;
						int64 OldCounter = pValue->f_Integer();
						if (NewCounter <= OldCounter)
							co_return DMibErrorInstance("U2F counter decreased ({} <= {}), has your key been copied?"_f << NewCounter << OldCounter);
					}

					Return.m_UpdatedPublicData["Counter"] = Result.m_Counter;

					co_return fg_Move(Return);
				}
			)
		;
	}

	class CDistributedActorTrustManagerAuthenticationActorFactoryU2F : public ICDistributedActorTrustManagerAuthenticationActorFactory
	{
		CAuthenticationActorInfo operator () (TCActor<CDistributedActorTrustManager> const &_TrustManager) override;
		static mint ms_MakeActive;
	};
	
	mint CDistributedActorTrustManagerAuthenticationActorFactoryU2F::ms_MakeActive;

	CAuthenticationActorInfo CDistributedActorTrustManagerAuthenticationActorFactoryU2F::operator ()(TCActor<CDistributedActorTrustManager> const &_TrustManager)
	{
		return CAuthenticationActorInfo
			{
				fg_Construct<CDistributedActorTrustManagerAuthenticationActorU2F>(_TrustManager)
				, EAuthenticationFactorCategory_Possession
			}
		;
	}

	DMibAuthenticationFactorRegister(CDistributedActorTrustManagerAuthenticationActorFactoryU2F);

	void fg_Malterlib_CDistributedActorTrustManagerAuthenticationActorU2F_MakeActive()
	{
		DMibRuntimeClassMakeActive(CDistributedActorTrustManagerAuthenticationActorFactoryU2F);
	}
}
