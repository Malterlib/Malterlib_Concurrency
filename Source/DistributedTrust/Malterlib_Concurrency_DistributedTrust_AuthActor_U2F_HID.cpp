// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include "Malterlib_Concurrency_DistributedTrust_AuthActor_U2F_HID.h"
#include "../../../../External/hidapi/hidapi/hidapi.h"

#ifdef DPlatformFamily_Windows
#pragma comment(lib, "Setupapi.lib")
#endif

namespace NMib::NConcurrency
{
	DMibImpErrorClassImplement(CExceptionHidapiUSB);

	using namespace NContainer;
	using namespace NStr;

	namespace
	{
		struct CSubSystem_Concurrency_U2FHID : public CSubSystem
		{
			~CSubSystem_Concurrency_U2FHID()
			{
				if (m_Actor)
					m_Actor->f_BlockDestroy();
				m_Actor.f_Clear();
			}

			void f_PreDestroyThreadSpecific() override
			{
				if (m_Actor)
					m_Actor->f_BlockDestroy();
				m_Actor.f_Clear();
			}

			TCActor<CHumanInterfaceDevicesActor> m_Actor = {fg_Construct(), "Human Interface Device Actor"};
		};

		constinit TCSubSystem<CSubSystem_Concurrency_U2FHID, ESubSystemDestruction_BeforeMemoryManager>
			g_MalterlibSubSystem_Concurrency_U2FHID = {DAggregateInit}
		;

		//template <typename tf_CStr>
		auto fg_ToHidAPIStr(NStr::CStr const &_String)
		{
			if constexpr (sizeof(wchar_t) == 1)
				return NStr::CStr(_String);
			else if constexpr (sizeof(wchar_t) == 2)
				return NStr::CWStr(_String);
			else
				return NStr::CUStr(_String);
		}

		NStr::CStr fg_FromHidAPIStr(wchar_t const *_pString)
		{
			if constexpr (sizeof(wchar_t) == 1)
				return NStr::CStr((ch8 const *)_pString);
			else if constexpr (sizeof(wchar_t) == 2)
				return NStr::CWStr((ch16 const *)_pString);
			else
				return NStr::CUStr((ch32 const *)_pString);
		}
	}

	TCActor<CHumanInterfaceDevicesActor> fg_HumanInterfaceDevicesActor()
	{
		return g_MalterlibSubSystem_Concurrency_U2FHID->m_Actor;
	}

	NException::CExceptionPointer CHumanInterfaceDevicesActor::fp_CheckInit()
	{
		if (mp_InitError)
			return DMibErrorInstanceHID(mp_InitError);

		return nullptr;
	}

	CHumanInterfaceDevicesActor::CHumanInterfaceDevicesActor()
	{
		// Dispatch to self so init is run on our thread
		g_Dispatch / [this]
			{
				if (hid_init())
					mp_InitError = "HID initalization failed (hid_init)";
			}
			> g_DiscardResult
		;
	}

	CHumanInterfaceDevicesActor::~CHumanInterfaceDevicesActor()
	{
		hid_exit();
	}

	TCFuture<TCVector<CHumanInterfaceDevicesActor::CDeviceInfo>> CHumanInterfaceDevicesActor::f_Enumerate(uint16 _VendorID, uint16 _ProductID)
	{
		if (auto pError = fp_CheckInit())
			co_return fg_Move(pError);

		TCVector<CDeviceInfo> Result;
		auto *pStartofEnumeration = hid_enumerate(_VendorID, _ProductID);
		auto Cleanup = g_OnScopeExit / [&]
			{
				hid_free_enumeration(pStartofEnumeration);
			}
		;

		for (auto *pDeviceInfo = pStartofEnumeration; pDeviceInfo; pDeviceInfo = pDeviceInfo->next)
		{
			Result.f_Insert
				(
					{
						pDeviceInfo->path
						, pDeviceInfo->vendor_id
						, pDeviceInfo->product_id
						, pDeviceInfo->serial_number
						, pDeviceInfo->release_number
						, pDeviceInfo->manufacturer_string
						, pDeviceInfo->product_string
						, pDeviceInfo->usage_page
						, pDeviceInfo->usage
					}
				)
			;
		}

		co_return fg_Move(Result);
	}

	auto CHumanInterfaceDevicesActor::f_Open(uint16 _VendorID, uint16 _ProductID, NStr::CStr _SerialNumber) -> TCFuture<TCActor<CHumanInterfaceDeviceActor>>
	{
		if (auto pError = fp_CheckInit())
			co_return fg_Move(pError);

		auto SerialNumber = fg_ToHidAPIStr(_SerialNumber);
		auto pDevice = hid_open(_VendorID, _ProductID, (wchar_t const *)(SerialNumber ? SerialNumber.f_GetStr() : nullptr));
		if (!pDevice)
			co_return DMibErrorInstanceHID("hid_open error");

		TCActor<CHumanInterfaceDeviceActor> DeviceActor = {fg_Construct(pDevice), fg_ThisActor(this)};
		co_return fg_Move(DeviceActor);
	}

	auto CHumanInterfaceDevicesActor::f_OpenPath(NStr::CStr _Path) -> TCFuture<TCActor<CHumanInterfaceDeviceActor>>
	{
		if (auto pError = fp_CheckInit())
			co_return fg_Move(pError);

		auto pDevice = hid_open_path(_Path.f_GetStr());
		if (!pDevice)
			co_return DMibErrorInstanceHID("hid_open error");

		TCActor<CHumanInterfaceDeviceActor> DeviceActor = {fg_Construct(pDevice), fg_ThisActor(this)};
		co_return fg_Move(DeviceActor);
	}

	CHumanInterfaceDeviceActor::CHumanInterfaceDeviceActor(hid_device *_Device)
		: m_pDevice(_Device)
	{
	}


	CHumanInterfaceDeviceActor::~CHumanInterfaceDeviceActor()
	{
		if (m_pDevice)
			hid_close(m_pDevice);
		m_pDevice = nullptr;
	}

	TCFuture<aint> CHumanInterfaceDeviceActor::f_Write(uint8 _ReportID, NContainer::CSecureByteVector _Buffer)
	{
		_Buffer.f_InsertFirst(_ReportID);
		int nBytes = hid_write(m_pDevice, _Buffer.f_GetArray(), _Buffer.f_GetLen());
		if (nBytes == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to write to device (hid_write)"));

		co_return nBytes;
	}

	TCFuture<NContainer::CSecureByteVector> CHumanInterfaceDeviceActor::f_Read(size_t _Length)
	{
		NContainer::CSecureByteVector Buffer;
		int nBytes = hid_read(m_pDevice, Buffer.f_GetArray(_Length), _Length);
		if (nBytes == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to read from device (hid_read)"));

		co_return Buffer;
	}

	TCFuture<NContainer::CSecureByteVector> CHumanInterfaceDeviceActor::f_ReadTimeout(size_t _Length, int _TimeoutMilliseconds)
	{
		NContainer::CSecureByteVector Buffer;
		int nBytes = hid_read_timeout(m_pDevice, Buffer.f_GetArray(_Length), _Length, _TimeoutMilliseconds);
		if (nBytes == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to read from device (hid_read_timeout)"));
		Buffer.f_SetLen(nBytes);

		co_return Buffer;
	}

	TCFuture<void> CHumanInterfaceDeviceActor::f_SendFeatureReport(uint8 _ReportID, NContainer::CSecureByteVector _Buffer)
	{
		_Buffer.f_InsertFirst(_ReportID);
		int nBytes = hid_send_feature_report(m_pDevice, _Buffer.f_GetArray(), _Buffer.f_GetLen());
		if (nBytes == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get feature report (hid_send_feature_report)"));

		co_return {};
	}

	TCFuture<NContainer::CSecureByteVector> CHumanInterfaceDeviceActor::f_GetFeatureReport(uint8 _ReportID)
	{
		NContainer::CSecureByteVector Buffer;
		Buffer.f_SetLen(1024);
		Buffer[0] = _ReportID;

		int nBytes = hid_get_feature_report(m_pDevice, Buffer.f_GetArray(), 1024);
		if (nBytes == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get feature report (hid_get_feature_report)"));

		Buffer.f_SetLen(nBytes);
		co_return fg_Move(Buffer);
	}

	TCFuture<NStr::CStr> CHumanInterfaceDeviceActor::f_GetManufacturerString() const
	{
		wchar_t Buffer[1024];
		int Result = hid_get_manufacturer_string(m_pDevice, Buffer, 1024);
		if (Result == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get manufacturer string (hid_get_manufacturer_string)"));
		co_return fg_FromHidAPIStr(Buffer);
	}

	TCFuture<NStr::CStr> CHumanInterfaceDeviceActor::f_GetProductString() const
	{
		wchar_t Buffer[1024];
		int Result = hid_get_product_string(m_pDevice, Buffer, 1024);
		if (Result == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get product string (hid_get_product_string)"));
		co_return fg_FromHidAPIStr(Buffer);
	}

	TCFuture<NStr::CStr> CHumanInterfaceDeviceActor::f_GetSerialNumberString() const
	{
		wchar_t Buffer[1024];
		int Result = hid_get_serial_number_string(m_pDevice, Buffer, 1024);
		if (Result == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get serial number string(hid_get_serial_number_string)"));
		co_return fg_FromHidAPIStr(Buffer);
	}

	TCFuture<NStr::CStr> CHumanInterfaceDeviceActor::f_GetIndexedString(aint _StringIndex) const
	{
		wchar_t Buffer[1024];
		int Result = hid_get_indexed_string(m_pDevice, _StringIndex, Buffer, 1024);
		if (Result == -1)
			co_return DMibErrorInstanceHID(fp_Error("Failed to get indexed string (hid_get_indexed_string)"));
		co_return fg_FromHidAPIStr(Buffer);
	}

	NStr::CStr CHumanInterfaceDeviceActor::fp_Error(NStr::CStr const &_Description) const
	{
		if (wchar_t const *pError = hid_error(m_pDevice))
			return "{}: {}"_f << _Description << fg_FromHidAPIStr(pError);
		else
			return _Description;
	}
}
