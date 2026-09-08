// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <Mib/Concurrency/DistributedApp>
#include <Mib/Stream/Streams/Vector>
#include <Mib/Test/Exception>

namespace NMib::NConcurrency
{
	struct CCommandLineClientInfo_Tests : NTest::CTest
	{
		void f_DoTests()
		{
			DMibTestSuite("UTCOffsetPresence")
			{
				DMibExpectFalse(CCommandLineClientInfo{}.m_UTCOffsetSeconds);
				DMibExpectFalse(CCommandLineClientInfo::fs_CollectLocal(false).m_UTCOffsetSeconds);
				DMibExpectTrue(CCommandLineClientInfo::fs_CollectLocal(true).m_UTCOffsetSeconds);

				auto fCheck = [](NStr::CStr const &_Case, NStorage::TCOptional<int32> const &_Offset)
					{
						DMibTestPath(_Case);

						for (uint32 Version : {uint32(ICCommandLine::EProtocolVersion_SupportClientInfo), uint32(ICCommandLine::EProtocolVersion_SupportOptionalUTCOffset)})
						{
							DMibTestPath(NStr::fg_Format("{}", Version));

							CCommandLineClientInfo Info;
							Info.m_UTCOffsetSeconds = _Offset;
							Info.m_bClipboardSupported = true;
							NStream::CBinaryStreamMemory<> Stream;
							NStream::CScopeBinaryStreamVersion StreamVersion(Stream, Version);
							Stream << Info;
							Stream.f_SetPosition(0);

							CCommandLineClientInfo Result;
							Result.m_UTCOffsetSeconds = int32(123);
							Stream >> Result;

							bool bExpectedSet = bool(_Offset) || Version < ICCommandLine::EProtocolVersion_SupportOptionalUTCOffset;
							DMibExpect(bool(Result.m_UTCOffsetSeconds), ==, bExpectedSet);
							if (Result.m_UTCOffsetSeconds)
								DMibExpect(*Result.m_UTCOffsetSeconds, ==, _Offset.f_Get(int32(0)));
							DMibExpectTrue(Result.m_bClipboardSupported);
							DMibExpect(Stream.f_GetPosition(), ==, Stream.f_GetLength());
						}
					}
				;

				fCheck("Unset", {});
				fCheck("UTC", int32(0));
				fCheck("East", int32(19800));
				fCheck("West", int32(-18000));
			};
		}
	};

	DMibTestRegister(CCommandLineClientInfo_Tests, Malterlib::Concurrency);
}
