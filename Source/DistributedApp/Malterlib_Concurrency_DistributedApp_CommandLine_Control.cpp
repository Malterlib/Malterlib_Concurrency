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
		DMibPublishActorFunction(ICCommandLineControl::f_RegisterForCancellation);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadBinary);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadLine);
		DMibPublishActorFunction(ICCommandLineControl::f_ReadPrompt);
		DMibPublishActorFunction(ICCommandLineControl::f_AbortReads);
		DMibPublishActorFunction(ICCommandLineControl::f_StdOut);
		DMibPublishActorFunction(ICCommandLineControl::f_StdOutBinary);
		DMibPublishActorFunction(ICCommandLineControl::f_StdErr);
		DMibPublishActorFunction(ICCommandLineControl::f_U2F_Register);
		DMibPublishActorFunction(ICCommandLineControl::f_U2F_Authenticate);
	}

	ICCommandLineControl::~ICCommandLineControl() = default;

	TCFuture<void> CCommandLineControl::f_StdOut(CStrIO const &_Output) const
	{
		if (!m_ControlActor)
			return g_Void;

		mint OutputLen = _Output.f_GetLen();
		if (OutputLen <= CActorDistributionManager::mc_HalfMaxMessageSize)
			return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_Output);

		TCFutureVector<void> Results;
		NMisc::fg_ChunkRange
			(
				mint(0)
				, OutputLen
				, CActorDistributionManager::mc_HalfMaxMessageSize
				, [&](mint _Start, mint _Len)
				{
					m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOut)(_Output.f_Extract(_Start, _Len)) > Results;
				}
			)
		;

		TCPromiseFuturePair<void> Promise;
		fg_AllDoneWrapped(Results).f_OnResultSet(fg_Move(Promise.m_Promise).f_ReceiveAnyUnwrap());
		return fg_Move(Promise.m_Future);
	}

	TCFuture<void> CCommandLineControl::fsp_SendStdOutBinary(CCommandLineControl const &_This, uint8 const *_pData, mint _DataLen)
	{
		TCFutureVector<void> Results;
		NMisc::fg_ChunkRange
			(
				mint(0)
				, _DataLen
				, CActorDistributionManager::mc_HalfMaxMessageSize
				, [&](mint _Start, mint _Len)
				{
					CIOByteVector ToSend;
					ToSend.f_Insert(_pData + _Start, _Len);
					_This.m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(fg_Move(ToSend)) > Results;
				}
			)
		;

		TCPromiseFuturePair<void> Promise;
		fg_AllDoneWrapped(Results).f_OnResultSet(fg_Move(Promise.m_Promise).f_ReceiveAnyUnwrap());

		return fg_Move(Promise.m_Future);
	}

	TCFuture<void> CCommandLineControl::f_StdOutBinary(CSecureByteVector const &_Output) const
	{
		if (!m_ControlActor)
			return g_Void;

		mint OutputLen = _Output.f_GetLen();
		if (OutputLen <= CActorDistributionManager::mc_HalfMaxMessageSize)
			return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(CIOByteVector::fs_AllowInsecureConversion(_Output));

		return fsp_SendStdOutBinary(*this, _Output.f_GetArray(), OutputLen);
	}

	TCFuture<void> CCommandLineControl::f_StdOutBinary(CByteVector const &_Output) const
	{
		if (!m_ControlActor)
			return g_Void;

		mint OutputLen = _Output.f_GetLen();
		if (OutputLen <= CActorDistributionManager::mc_HalfMaxMessageSize)
			return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdOutBinary)(CIOByteVector::fs_AllowInsecureConversion(_Output));

		return fsp_SendStdOutBinary(*this, _Output.f_GetArray(), OutputLen);
	}

	TCFuture<void> CCommandLineControl::f_StdErr(CStrIO const &_Output) const
	{
		if (!m_ControlActor)
			return g_Void;

		mint OutputLen = _Output.f_GetLen();
		if (OutputLen <= CActorDistributionManager::mc_HalfMaxMessageSize)
			return m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_Output);

		TCFutureVector<void> Results;
		NMisc::fg_ChunkRange
			(
				mint(0)
				, OutputLen
				, CActorDistributionManager::mc_HalfMaxMessageSize
				, [&](mint _Start, mint _Len)
				{
					m_ControlActor.f_CallActor(&ICCommandLineControl::f_StdErr)(_Output.f_Extract(_Start, _Len)) > Results;
				}
			)
		;

		TCPromiseFuturePair<void> Promise;
		fg_AllDoneWrapped(Results).f_OnResultSet(fg_Move(Promise.m_Promise).f_ReceiveAnyUnwrap());
		return fg_Move(Promise.m_Future);
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
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdInBinary)(fg_Move(_fOnBinaryInput), _Flags);
	}

	auto CCommandLineControl::f_RegisterForCancellation(ICCommandLineControl::FOnCancel &&_fOnCancel) const -> TCFuture<TCActorSubscriptionWithID<>>
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForCancellation)(fg_Move(_fOnCancel));
	}

	auto CCommandLineControl::f_RegisterForStdIn(ICCommandLineControl::FOnInput &&_fOnInput, EStdInReaderFlag _Flags) const
		-> TCFuture<TCActorSubscriptionWithID<>>
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_RegisterForStdIn)(fg_Move(_fOnInput), _Flags);
	}

	TCFuture<CIOByteVector> CCommandLineControl::f_ReadBinary() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadBinary)();
	}

	TCFuture<CStrIO> CCommandLineControl::f_ReadLine() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadLine)();
	}

	TCFuture<CStrIO> CCommandLineControl::f_ReadPrompt(CStdInReaderPromptParams const &_Params) const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_ReadPrompt)(_Params);
	}

	NConcurrency::TCFuture<ICCommandLineControl::CU2FRegister::CResult> CCommandLineControl::f_U2F_Register(ICCommandLineControl::CU2FRegister &&_Register)
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_U2F_Register)(fg_Move(_Register));
	}

	NConcurrency::TCFuture<ICCommandLineControl::CU2FAuthenticate::CResult> CCommandLineControl::f_U2F_Authenticate(ICCommandLineControl::CU2FAuthenticate &&_Authenticate)
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_U2F_Authenticate)(fg_Move(_Authenticate));
	}

	TCFuture<void> CCommandLineControl::f_AbortReads() const
	{
		if (!m_ControlActor)
			return DMibErrorInstance("No control actor");
		return m_ControlActor.f_CallActor(&ICCommandLineControl::f_AbortReads)();
	}

	uint32 CCommandLineControl::f_AddAsyncResult(CAsyncResult const &_Result) const
	{
		if (!_Result)
		{
			(void)f_StdErr(fg_Format<CStrIO>("{}\n", _Result.f_GetExceptionStr()));
			return 1;
		}
		return 0;
	}

	void CCommandLineControl::operator += (ch8 const *_pStdOut) const
	{
		f_StdOut(_pStdOut).f_DiscardResult();
	}

	void CCommandLineControl::operator %= (ch8 const *_pStdErr) const
	{
		f_StdErr(_pStdErr).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CStr const &_StdOut) const
	{
		f_StdOut(_StdOut).f_DiscardResult();
	}

	void CCommandLineControl::operator %= (CStr const &_StdErr) const
	{
		f_StdErr(_StdErr).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CStrSecure const &_StdOut) const
	{
		f_StdOut(_StdOut).f_DiscardResult();
	}

	void CCommandLineControl::operator %= (CStrSecure const &_StdErr) const
	{
		f_StdErr(_StdErr).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CStr::CFormat const &_StdOut) const
	{
		f_StdOut(CStr(_StdOut)).f_DiscardResult();
	}

	void CCommandLineControl::operator %= (CStr::CFormat const &_StdErr) const
	{
		f_StdErr(CStr(_StdErr)).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CStrSecure::CFormat const &_StdOut) const
	{
		f_StdOut(CStrSecure(_StdOut)).f_DiscardResult();
	}

	void CCommandLineControl::operator %= (CStrSecure::CFormat const &_StdErr) const
	{
		f_StdErr(CStrSecure(_StdErr)).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CByteVector const &_StdOut) const
	{
		f_StdOutBinary(_StdOut).f_DiscardResult();
	}

	void CCommandLineControl::operator += (CSecureByteVector const &_StdOut) const
	{
		f_StdOutBinary(_StdOut).f_DiscardResult();
	}
}
