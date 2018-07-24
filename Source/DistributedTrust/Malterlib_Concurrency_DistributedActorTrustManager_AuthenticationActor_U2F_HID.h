// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#pragma once

#include <Mib/Core/Core>
#include <Mib/Container/Vector>
#include <Mib/Exception/Exception>
#include <Mib/Concurrency/ConcurrencyManager>

extern "C"
{
	struct hid_device_;
	typedef struct hid_device_ hid_device; /**< opaque hidapi structure */
}

namespace NMib::NConcurrency
{
	DMibImpErrorClass(CExceptionHidapiUSB, NException::CException);
#	define DMibErrorHID(d_Description) DMibImpError(NMib::NConcurrency::CExceptionHidapiUSB, d_Description)
#	define DMibErrorInstanceHID(d_Description) DMibImpExceptionInstance(NMib::NConcurrency::CExceptionHidapiUSB, d_Description)


#	ifndef DMibPNoShortCuts
#		define DErrorHID DMibErrorHID
#		define DErrorInstanceHID DMibErrorInstanceHID
#	endif

	struct CHumanInterfaceDevicesActor : public CActor
	{
		using CActorHolder = CSeparateThreadActorHolder;

		enum
		{
			EReportSize = 64 	// Default size of raw HID report
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

		struct CDevice : public CActor
		{
			using CActorHolder = CDelegatedActorHolder;

			CDevice(hid_device *_Device);
			~CDevice();

			TCContinuation<aint> f_Write(uint8 _ReportID, NContainer::CByteVector &&_Buffer);
			TCContinuation<NContainer::CByteVector> f_Read(size_t _Length);
			TCContinuation<NContainer::CByteVector> f_ReadTimeout(size_t _Length, int _TimeoutMilliseconds);

			TCContinuation<void> f_SendFeatureReport(uint8 _ReportID, NContainer::CByteVector &&_Buffer);
			TCContinuation<NContainer::CByteVector> f_GetFeatureReport(uint8 _ReportID);

			TCContinuation<NStr::CStr> f_GetManufacturerString() const;
			TCContinuation<NStr::CStr> f_GetProductString() const;
			TCContinuation<NStr::CStr> f_GetSerialNumberString() const;
			TCContinuation<NStr::CStr> f_GetIndexedString(aint _StringIndex) const;

		private:

			NStr::CStr fp_Error(NStr::CStr const &_Description) const;
			hid_device *m_pDevice = nullptr;
		};

		CHumanInterfaceDevicesActor();
		~CHumanInterfaceDevicesActor();

		TCContinuation<NContainer::TCVector<CDeviceInfo>> f_Enumerate(uint16 _VendorID, uint16 _ProductID);
		TCContinuation<TCActor<CDevice>> f_Open(uint16 _VendorID, uint16 _ProductID, NStr::CStr const &_SerialNumber = "");
		TCContinuation<TCActor<CDevice>> f_OpenPath(NStr::CStr const &_Path);

	private:
		template <typename tf_CReturn>
		bool fp_CheckInit(TCContinuation<tf_CReturn> &_Continuation);

		NStr::CStr mp_InitError;
	};

	TCActor<CHumanInterfaceDevicesActor> fg_HumanInterfaceDevicesActor();
}
