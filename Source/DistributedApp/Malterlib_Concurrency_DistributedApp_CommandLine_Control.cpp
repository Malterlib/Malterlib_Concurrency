// Copyright © 2015 Hansoft AB
// Distributed under the MIT license, see license text in LICENSE.Malterlib

#include <Mib/Core/Core>
#include <Mib/CommandLine/TableRenderer>

#include "Malterlib_Concurrency_DistributedApp.h"

namespace NMib::NConcurrency
{
	using namespace NStr;
	using namespace NContainer;
	using namespace NCommandLine;
	using namespace NProcess;

	ICCommandLine::ICCommandLine()
	{
		DMibPublishActorFunction(ICCommandLine::f_RunCommandLine);
	}

	ICCommandLineControl::ICCommandLineControl()
	{
		DMibPublishActorFunction(ICCommandLineControl::f_RegisterForStdIn);
		DMibPublishActorFunction(ICCommandLineControl::f_RegisterForStdInBinary);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadBinary);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadLine);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadPrompt);
		DMibPublishActorFunction(ICCommandLineControl::f_AbortReads);
		DMibPublishActorFunction(ICCommandLineControl::f_StdOut);
		DMibPublishActorFunction(ICCommandLineControl::f_StdOutBinary);
		DMibPublishActorFunction(ICCommandLineControl::f_StdErr);
	}

	ICCommandLineControl::~ICCommandLineControl() = default;

	TCFuture<void> CCommandLineControl::f_StdOut(CStrSecure const &_Output) const
	{
		TCPromise<void> Promise;
		if (!m_ControlActor)
			return Promise <<= g_Void;
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_Output);
	}

	TCFuture<void> CCommandLineControl::f_StdOutBinary(CSecureByteVector const &_Output) const
	{
		TCPromise<void> Promise;
		if (!m_ControlActor)
			return Promise <<= g_Void;
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(_Output);
	}

	TCFuture<void> CCommandLineControl::f_StdErr(CStrSecure const &_Output) const
	{
		TCPromise<void> Promise;
		if (!m_ControlActor)
			return Promise <<= g_Void;
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_Output);
	}

	CTableRenderHelper CCommandLineControl::f_TableRenderer() const
	{
		return CTableRenderHelper
			(
				[this](CStr const &_Output)
			 	{
					*this += _Output;
				}
			 	, CTableRenderHelper::EOption_Rounded
			 	, m_AnsiFlags
		 		, m_CommandLineWidth
		 	)
		;
	}

	CAnsiEncoding CCommandLineControl::f_AnsiEncoding() const
	{
		return CAnsiEncoding(m_AnsiFlags);
	}

	auto CCommandLineControl::f_RegisterForStdInBinary(ICCommandLineControl::FOnBinaryInput &&_fOnBinaryInput, EStdInReaderFlag _Flags) const
		-> TCFuture<TCActorSubscriptionWithID<>>
	{
		TCPromise<TCActorSubscriptionWithID<>> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdInBinary)(fg_Move(_fOnBinaryInput), _Flags);
	}

	auto CCommandLineControl::f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, EStdInReaderFlag _Flags) const
		-> TCFuture<TCActorSubscriptionWithID<>>
	{
		TCPromise<TCActorSubscriptionWithID<>> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdIn)(fg_Move(_fOnInput), _Flags);
	}

	TCFuture<CSecureByteVector> CCommandLineControl::f_ReadBinary() const
	{
		TCPromise<CSecureByteVector> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadBinary)();
	}

	TCFuture<CStrSecure> CCommandLineControl::f_ReadLine() const
	{
		TCPromise<CStrSecure> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadLine)();
	}

	TCFuture<CStrSecure> CCommandLineControl::f_ReadPrompt(CStdInReaderPromptParams const &_Params) const
	{
		TCPromise<CStrSecure> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadPrompt)(_Params);
	}

	TCFuture<void> CCommandLineControl::f_AbortReads() const
	{
		TCPromise<void> Promise;
		if (!m_ControlActor)
			return Promise <<= DMibErrorInstance("No control actor");
		return Promise <<= m_ControlActor.f_CallActor(&ICCommandLineControl::f_AbortReads)();
	}

	uint32 CCommandLineControl::f_AddAsyncResult(CAsyncResult const &_Result) const
	{
		if (!_Result)
		{
			(void)f_StdErr(fg_Format<CStrSecure>("{}\n", _Result.f_GetExceptionStr()));
			return 1;
		}
		return 0;
	}

	void CCommandLineControl::operator +=(CStrSecure const &_StdOut) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_StdOut) > fg_DiscardResult();
	}

	void CCommandLineControl::operator %=(CStrSecure const &_StdErr) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_StdErr) > fg_DiscardResult();
	}

	void CCommandLineControl::operator +=(CStr::CFormat const &_StdOut) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(CStr{_StdOut}) > fg_DiscardResult();
	}

	void CCommandLineControl::operator %=(CStr::CFormat const &_StdErr) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(CStr{_StdErr}) > fg_DiscardResult();
	}

	void CCommandLineControl::operator +=(CStrSecure::CFormat const &_StdOut) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_StdOut) > fg_DiscardResult();
	}

	void CCommandLineControl::operator %=(CStrSecure::CFormat const &_StdErr) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_StdErr) > fg_DiscardResult();
	}

	void CCommandLineControl::operator +=(CSecureByteVector const &_StdOut) const
	{
		if (!m_ControlActor)
			return;
		m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(_StdOut) > fg_DiscardResult();
	}
}
