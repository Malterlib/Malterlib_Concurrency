// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/Vector>
#include <Mib/Exception/Exception>
#include <Mib/Concurrency/ConcurrencyManager>

extern "C"
{
	struct hid_device_;
	using hid_device = struct hid_device_; /**< opaque hidapi structure */
}

namespace NMib::NConcurrency
{
	DMibImpErrorClassDefine(CExceptionHidapiUSB, NException::CException);
#	define DMibErrorHID(d_Description) DMibImpError(NMib::NConcurrency::CExceptionHidapiUSB, d_Description)
#	define DMibErrorInstanceHID(d_Description) DMibImpExceptionInstance(NMib::NConcurrency::CExceptionHidapiUSB, d_Description)


#	ifndef DMibPNoShortCuts
#		define DErrorHID DMibErrorHID
#		define DErrorInstanceHID DMibErrorInstanceHID
#	endif

	struct CHumanInterfaceDeviceActor : public CActor
	{
		using CActorHolder = CDelegatedActorHolder;

		CHumanInterfaceDeviceActor(hid_device *_Device);
		~CHumanInterfaceDeviceActor();

		TCFuture<aint> f_Write(uint8 _ReportID, NContainer::CSecureByteVector _Buffer);
		TCFuture<NContainer::CSecureByteVector> f_Read(size_t _Length);
		TCFuture<NContainer::CSecureByteVector> f_ReadTimeout(size_t _Length, int _TimeoutMilliseconds);

		TCFuture<void> f_SendFeatureReport(uint8 _ReportID, NContainer::CSecureByteVector _Buffer);
		TCFuture<NContainer::CSecureByteVector> f_GetFeatureReport(uint8 _ReportID);

		TCFuture<NStr::CStr> f_GetManufacturerString() const;
		TCFuture<NStr::CStr> f_GetProductString() const;
		TCFuture<NStr::CStr> f_GetSerialNumberString() const;
		TCFuture<NStr::CStr> f_GetIndexedString(aint _StringIndex) const;

	private:

		NStr::CStr fp_Error(NStr::CStr const &_Description) const;
		hid_device *m_pDevice = nullptr;
	};

	struct CHumanInterfaceDevicesActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		enum
		{
			EReportSize = 64	// Default size of raw HID report
		};

		struct CDeviceInfo
		{
			NStr::CStr m_Path;
			uint16 m_VendorID;
			uint16 m_ProductID;
			NStr::CStr m_SerialNumber;
			uint16 m_ReleaseNumber;
			NStr::CStr m_ManufacturerString;
			NStr::CStr m_ProductString;
			uint16 m_UsagePage;
			uint16 m_Usage;
			aint m_InterfaceNumber;
		};

		CHumanInterfaceDevicesActor();
		~CHumanInterfaceDevicesActor();

		TCFuture<NContainer::TCVector<CDeviceInfo>> f_Enumerate(uint16 _VendorID, uint16 _ProductID);
		TCFuture<TCActor<CHumanInterfaceDeviceActor>> f_Open(uint16 _VendorID, uint16 _ProductID, NStr::CStr _SerialNumber = "");
		TCFuture<TCActor<CHumanInterfaceDeviceActor>> f_OpenPath(NStr::CStr _Path);

	private:
		NException::CExceptionPointer fp_CheckInit();

		NStr::CStr mp_InitError;
	};

	TCActor<CHumanInterfaceDevicesActor> fg_HumanInterfaceDevicesActor();
}
