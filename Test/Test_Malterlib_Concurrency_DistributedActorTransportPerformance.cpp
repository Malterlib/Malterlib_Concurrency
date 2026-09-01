// Copyright © Unbroken AB
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#define DMibRuntimeTypeRegistry

#include <Mib/Concurrency/RuntimeTypeRegistry>

#include <Mib/Test/Performance>

#include <Mib/Concurrency/ConcurrencyManager>
#include <Mib/Concurrency/DistributedActor>
#include <Mib/Concurrency/AsyncGenerator>
#include <Mib/Concurrency/DistributedActorTrustManager>
#include <Mib/Concurrency/DistributedActorTestHelpers>
#include <Mib/Concurrency/DistributedTrustTestHelpers>
#include <Mib/Network/Sockets/AuthenticatedUnix>
#include <Mib/File/File>

using namespace NMib;
using namespace NMib::NStorage;
using namespace NMib::NContainer;
using namespace NMib::NConcurrency;
using namespace NMib::NStr;
using namespace NMib::NTest;
using namespace NMib::NNetwork;

namespace
{
	uint64 fg_BenchSendWindowBytes()
	{
		static uint64 s_nBytes =
			(
				[]() -> uint64
				{
					CStr Text = fg_GetSys()->f_GetEnvironmentVariable("SendWindow");
					uint64 nBytes = 0;
					CStr Error;
					if (!Text.f_IsEmpty() && !fg_ParseSendWindow(Text, nBytes, Error))
						DMibError("Invalid SendWindow: {}", Error);

					return nBytes;
				}
				()
			)
		;

		return s_nBytes;
	}
}

namespace NTestTransportPerformance
{
	static fp64 g_Timeout = 60.0 * gc_TimeoutMultiplier;

	static constexpr bool gc_bSerialGeneration = true;

	static constexpr uint32 gc_nPipelineLength = 16; // Outstanding chunk requests keep the pipeline fed through buffer-release latency.

	struct CTransportBenchActor : public CActor
	{
		enum : uint32
		{
			EProtocolVersion_Min = 0x101
			, EProtocolVersion_Current = 0x101
		};

		static constexpr ch8 const *mc_pDefaultNamespace = "com.malterlib/TransportBench";

		CTransportBenchActor()
		{
			DMibPublishActorFunction(CTransportBenchActor::f_Ping);
			DMibPublishActorFunction(CTransportBenchActor::f_GenerateData);
			DMibPublishActorFunction(CTransportBenchActor::f_GenerateDataStorage);
		}

		TCFuture<uint64> f_Ping(uint64 _Value)
		{
			co_return _Value;
		}

		TCAsyncGenerator<CSharedByteVector> f_GenerateData(uint64 _nBytes, uint32 _ChunkSize)
		{
			if constexpr (gc_bSerialGeneration)
			{
				// Reuse one buffer so the benchmark measures the transport, not buffer creation
				CSharedByteVector Chunk;
				{
					CIOByteVector Buffer;
					Buffer.f_SetLen(_ChunkSize);
					NMemory::fg_ObjectSet(Buffer.f_GetArray(), (uint8)0x3C, _ChunkSize);
					Chunk = CSharedByteVector(fg_Move(Buffer));
				}

				uint64 Position = 0;
				while (Position < _nBytes)
				{
					umint ThisTime = (umint)fg_Min<uint64>(_nBytes - Position, _ChunkSize);

					if (ThisTime == _ChunkSize)
						co_yield CSharedByteVector(Chunk);
					else
						co_yield CSharedByteVector(Chunk, 0, ThisTime);

					Position += ThisTime;
				}
			}
			else
			{
				// Each in-flight slot retains its own concurrent actor to bound the number of allocator arenas.
				umint nInFlight = fg_GetSys()->f_GetEnvironmentVariable("InFlightGen").f_ToInt(umint(2));

				TCVector<TCActor<CConcurrentActor>> SlotActors;
				for (umint iActor = 0; iActor < nInFlight; ++iActor)
					SlotActors.f_InsertLast(fg_ConcurrentActorLowPrio());

				uint64 SchedulePosition = 0;
				auto fScheduleNext = [&](umint _iSlot) -> TCFuture<CSharedByteVector>
					{
						umint ThisTime = (umint)fg_Min<uint64>(_nBytes - SchedulePosition, _ChunkSize);
						uint64 Position = SchedulePosition;
						SchedulePosition += ThisTime;

						return g_Dispatch(SlotActors[_iSlot])
							/ [Position, ThisTime]() -> CSharedByteVector
							{
								CIOByteVector Buffer;
								Buffer.f_SetLen(ThisTime);
								NMemory::fg_ObjectSet(Buffer.f_GetArray(), (uint8)(Position >> 8), ThisTime);

								return CSharedByteVector(fg_Move(Buffer));
							}
						;
					}
				;

				TCVector<TCFuture<CSharedByteVector>> InFlight;
				while (SchedulePosition < _nBytes && InFlight.f_GetLen() < nInFlight)
					InFlight.f_InsertLast(fScheduleNext(InFlight.f_GetLen()));

				uint64 nChunks = (_nBytes + _ChunkSize - 1) / _ChunkSize;
				for (uint64 iChunk = 0; iChunk < nChunks; ++iChunk)
				{
					umint iSlot = umint(iChunk % InFlight.f_GetLen());
					CSharedByteVector Data = co_await fg_Move(InFlight[iSlot]);

					if (SchedulePosition < _nBytes)
						InFlight[iSlot] = fScheduleNext(iSlot);

					co_yield fg_Move(Data);
				}
			}

			co_return {};
		}

		TCAsyncGenerator<NStream::CBinaryStorage> f_GenerateDataStorage(uint64 _nBytes, uint32 _ChunkSize)
		{
			CSharedByteVector Chunk;
			{
				CIOByteVector Buffer;
				Buffer.f_SetLen(_ChunkSize);
				NMemory::fg_ObjectSet(Buffer.f_GetArray(), (uint8)0x3C, _ChunkSize);
				Chunk = CSharedByteVector(fg_Move(Buffer));
			}

			uint64 Position = 0;
			while (Position < _nBytes)
			{
				umint ThisTime = (umint)fg_Min<uint64>(_nBytes - Position, _ChunkSize);

				NStream::CBinaryStorage Storage;
				if (ThisTime == _ChunkSize)
					Storage.f_AppendShared(CSharedByteVector(Chunk));
				else
					Storage.f_AppendShared(CSharedByteVector(Chunk, 0, ThisTime));
				co_yield fg_Move(Storage);

				Position += ThisTime;
			}

			co_return {};
		}
	};

	// Keep endpoint and socket actors on the same pool to avoid a wakeup between them.
	struct CTransportBenchActorHighCPU : public CTransportBenchActor
	{
		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;
	};

	struct CTransportBenchClientActor : public CActor
	{
		TCFuture<uint64> f_RunPing(TCDistributedActor<CTransportBenchActor> _Actor, umint _nRoundTrips)
		{
			uint64 Value = 0;
			for (umint i = 0; i < _nRoundTrips; ++i)
				Value = co_await _Actor.f_CallActor(&CTransportBenchActor::f_Ping)(Value + 1);

			co_return Value;
		}

		TCFuture<uint64> f_RunDownload(TCDistributedActor<CTransportBenchActor> _Actor, uint64 _nBytes, uint32 _ChunkSize)
		{
			auto Generator = co_await _Actor.f_CallActor(&CTransportBenchActor::f_GenerateData)(_nBytes, _ChunkSize);

			uint32 PipelineLength = fg_GetSys()->f_GetEnvironmentVariable("PipelineLength").f_ToInt(gc_nPipelineLength);

			uint64 nReceivedBytes = 0;
			for (auto iData = co_await fg_Move(Generator).f_GetPipelinedIterator(PipelineLength); iData; co_await ++iData)
				nReceivedBytes += (*iData).f_GetLen();

			co_return nReceivedBytes;
		}

		TCFuture<uint64> f_RunDownloadStorage(TCDistributedActor<CTransportBenchActor> _Actor, uint64 _nBytes, uint32 _ChunkSize)
		{
			auto Generator = co_await _Actor.f_CallActor(&CTransportBenchActor::f_GenerateDataStorage)(_nBytes, _ChunkSize);

			uint32 PipelineLength = fg_GetSys()->f_GetEnvironmentVariable("PipelineLength").f_ToInt(gc_nPipelineLength);

			uint64 nReceivedBytes = 0;
			for (auto iData = co_await fg_Move(Generator).f_GetPipelinedIterator(PipelineLength); iData; co_await ++iData)
				nReceivedBytes += (*iData).f_GetTotalLength();

			co_return nReceivedBytes;
		}
	};

	struct CTransportBenchClientActorHighCPU : public CTransportBenchClientActor
	{
		static constexpr EPriority mc_Priority = EPriority_NormalHighCPU;
	};

	struct CDistributedActorTransportPerformance_Tests : public NMib::NTest::CTest
	{
		struct CTransportBenchState
		{
			CTransportBenchState(CActorRunLoopTestHelper &_RunLoopHelper, CStr const &_Scheme, CStr const &_SuiteTag, int32 _ConnectionConcurrency, bool _bHighCPUServerActor = false)
				: mp_pRunLoop(_RunLoopHelper.m_pRunLoop)
			{
				CStr RootDirectory = NFile::CFile::fs_GetProgramDirectory() / ("DistributedActorTransportPerf_{}"_f << _SuiteTag);
				fg_TestAddCleanupPath(RootDirectory);

				mp_ServerState.m_DefaultSendWindowBytes = fg_BenchSendWindowBytes();
				m_ServerTrustManager = mp_ServerState.f_TrustManager("Server");

				CDistributedActorTrustManager_Address ServerAddress;

				// SENDMSG_ZC requires TCP; Unix sockets cannot exercise it.
				bool bTcp = fg_GetSys()->f_GetEnvironmentVariable("BenchTcp") == "1";
				if (bTcp)
					ServerAddress.m_URL = "{}://127.0.0.1:0/"_f << _Scheme; // Any port: the suites run as parallel processes, and the ticket carries the one the socket was given
				else
					ServerAddress.m_URL = "{}://[UNIX(666):{}]/"_f << _Scheme << fg_GetSafeUnixSocketPath("{}/{}_{}.sock"_f << RootDirectory << _SuiteTag << _Scheme);
				m_ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress, 0).f_CallSync(mp_pRunLoop, g_Timeout);

				if (bTcp)
				{
					auto BoundAddress = m_ServerTrustManager(&CDistributedActorTrustManager::f_GetListenAddress, ServerAddress).f_CallSync(mp_pRunLoop, g_Timeout);
					DMibExpect(BoundAddress.m_URL.f_GetPort(), !=, 0);
				}

				mp_pServerHelper = fg_Construct(m_ServerTrustManager, mp_pRunLoop);
				if (_bHighCPUServerActor)
				{
					mp_pServerHelper->f_Publish<CTransportBenchActor>
						(
							mp_pServerHelper->f_GetManager()->f_ConstructActor<CTransportBenchActorHighCPU>()
							, CTransportBenchActor::mc_pDefaultNamespace
						)
					;
				}
				else
				{
					mp_pServerHelper->f_Publish<CTransportBenchActor>
						(
							mp_pServerHelper->f_GetManager()->f_ConstructActor<CTransportBenchActor>()
							, CTransportBenchActor::mc_pDefaultNamespace
						)
					;
				}

				mp_ClientState.m_DefaultSendWindowBytes = fg_BenchSendWindowBytes();
				m_ClientTrustManager = mp_ClientState.f_TrustManager("Client");

				auto TrustTicket = m_ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr)
					.f_CallSync(mp_pRunLoop, g_Timeout)
				;
				m_ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, TrustTicket.m_Ticket, g_Timeout / 2, _ConnectionConcurrency, 0)
					.f_CallSync(mp_pRunLoop, g_Timeout)
				;

				mp_pClientHelper = fg_Construct(m_ClientTrustManager, mp_pRunLoop);

				CStr Subscription = mp_pClientHelper->f_Subscribe(CTransportBenchActor::mc_pDefaultNamespace);
				m_RemoteActor = mp_pClientHelper->f_GetRemoteActor<CTransportBenchActor>(Subscription);
			}

			~CTransportBenchState()
			{
				m_ClientTrustManager->f_BlockDestroy(mp_pRunLoop->f_ActorDestroyLoop());
				m_ServerTrustManager->f_BlockDestroy(mp_pRunLoop->f_ActorDestroyLoop());
			}

			TCSharedPointer<CDefaultRunLoop> mp_pRunLoop;
			CTrustManagerTestHelper mp_ServerState;
			TCActor<CDistributedActorTrustManager> m_ServerTrustManager;
			TCUniquePointer<CDistributedActorTestHelper> mp_pServerHelper;
			CTrustManagerTestHelper mp_ClientState;
			TCActor<CDistributedActorTrustManager> m_ClientTrustManager;
			TCUniquePointer<CDistributedActorTestHelper> mp_pClientHelper;
			TCDistributedActor<CTransportBenchActor> m_RemoteActor;
		};

		static bool fs_BenchSchemeEnabled(CStr const &_Tag)
		{
			CStr Schemes = fg_GetSys()->f_GetEnvironmentVariable("BenchSchemes");
			if (Schemes.f_IsEmpty())
				return true;

			return ("," + Schemes + ",").f_Find("," + _Tag + ",") >= 0;
		}

		template <typename tf_FMeasure>
		static void fs_MeasureSchemes(CTestPerformance &_PerfTest, tf_FMeasure const &_fMeasure)
		{
			// wsa requires a unix socket, so the TCP loopback mode measures wss only
			bool bTcp = fg_GetSys()->f_GetEnvironmentVariable("BenchTcp") == "1";

			if (!bTcp && fg_IsAuthenticatedUnixSupported() && fs_BenchSchemeEnabled("wsa"))
				_fMeasure("wsa");
			if (fs_BenchSchemeEnabled("wss"))
				_fMeasure("wss");

			DMibExpectTrue(_PerfTest);
		}

		template <typename t_CClientActor>
		static void fs_MeasurePing
			(
				CTestPerformance &_PerfTest
				, CActorRunLoopTestHelper &_RunLoopHelper
				, CStr const &_Scheme
				, CStr const &_SuiteTag
				, bool _bHighCPUServerActor
			)
		{
			DMibTestPath("{}_{}"_f << _SuiteTag << _Scheme);

			CTransportBenchState State(_RunLoopHelper, _Scheme, _SuiteTag, 1, _bHighCPUServerActor);
			TCActor<t_CClientActor> ClientActor = fg_ConstructActor<t_CClientActor>();

			ClientActor(&CTransportBenchClientActor::f_RunPing, State.m_RemoteActor, mc_nPingRoundTrips / 8)
				.f_CallSync(_RunLoopHelper.m_pRunLoop, g_Timeout)
			;

			CTestPerformanceMeasure Time("{}_{}"_f << _SuiteTag << _Scheme);
			for (umint iRepetition = 0; iRepetition < mc_nRepetitions; ++iRepetition)
			{
				Time.f_Start();
				ClientActor(&CTransportBenchClientActor::f_RunPing, State.m_RemoteActor, mc_nPingRoundTrips)
					.f_CallSync(_RunLoopHelper.m_pRunLoop, g_Timeout)
				;
				Time.f_Stop(mc_nPingRoundTrips);
			}
			_PerfTest.f_Add(Time);
		}

		static void fs_MeasureThroughputOnActor
			(
				CTestPerformance &_PerfTest
				, CActorRunLoopTestHelper &_RunLoopHelper
				, TCDistributedActor<CTransportBenchActor> const &_RemoteActor
				, CStr const &_MeasureName
				, int32 _ConnectionConcurrency
				, fp64 _CallTimeout
			)
		{
			uint32 ChunkSize = fg_GetSys()->f_GetEnvironmentVariable("ChunkSize").f_ToInt(mc_ChunkSize);
			uint64 TransferBytes = fg_GetSys()->f_GetEnvironmentVariable("TransferBytes").f_ToInt(mc_nTransferBytes);

			TCActor<CTransportBenchClientActor> ClientActor = fg_ConstructActor<CTransportBenchClientActor>();

			bool bStorageChunks = fg_GetSys()->f_GetEnvironmentVariable("BenchStorage") != "0";
			auto fDownload = [&](uint64 _nBytes) -> uint64
				{
					if (bStorageChunks)
					{
						return ClientActor(&CTransportBenchClientActor::f_RunDownloadStorage, _RemoteActor, _nBytes, ChunkSize)
							.f_CallSync(_RunLoopHelper.m_pRunLoop, _CallTimeout)
						;
					}

					return ClientActor(&CTransportBenchClientActor::f_RunDownload, _RemoteActor, _nBytes, ChunkSize)
						.f_CallSync(_RunLoopHelper.m_pRunLoop, _CallTimeout)
					;
				}
			;

			uint64 nWarmupBytes = fDownload((uint64)ChunkSize);
			DMibExpect(nWarmupBytes, ==, (uint64)ChunkSize);

			CTestPerformanceMeasure Time(_MeasureName);
			uint64 nReceivedBytes = 0;
			for (umint iRepetition = 0; iRepetition < mc_nRepetitions; ++iRepetition)
			{
				Time.f_Start();
				nReceivedBytes += fDownload(TransferBytes);
				Time.f_Stop(TransferBytes, _ConnectionConcurrency);
			}
			_PerfTest.f_Add(Time);

			DMibExpect(nReceivedBytes, ==, TransferBytes * mc_nRepetitions);
		}

		static void fs_MeasureThroughput
			(
				CTestPerformance &_PerfTest
				, CActorRunLoopTestHelper &_RunLoopHelper
				, CStr const &_Scheme
				, CStr const &_SuiteTag
				, int32 _ConnectionConcurrency
			)
		{
			DMibTestPath(_Scheme);

			CTransportBenchState State(_RunLoopHelper, _Scheme, _SuiteTag, _ConnectionConcurrency);

			fs_MeasureThroughputOnActor
				(
					_PerfTest
					, _RunLoopHelper
					, State.m_RemoteActor
					, "{}_{}"_f << _SuiteTag << _Scheme
					, _ConnectionConcurrency
					, g_Timeout
				)
			;
		}

		void f_DoTests()
		{
			DMibTestSuite(CTestCategory("TransportPing") << CTestGroup("Performance"))
			{
				CActorRunLoopTestHelper RunLoopHelper;

				CTestPerformance PerfTest(0.015);

				fs_MeasureSchemes
					(
						PerfTest
						, [&](CStr const &_Scheme)
						{
							fs_MeasurePing<CTransportBenchClientActor>(PerfTest, RunLoopHelper, _Scheme, "Ping", false);
							fs_MeasurePing<CTransportBenchClientActorHighCPU>(PerfTest, RunLoopHelper, _Scheme, "PingHighCPU", true);
						}
					)
				;
			};

			DMibTestSuite(CTestCategory("TransportThroughput") << CTestGroup("Performance"))
			{
				CActorRunLoopTestHelper RunLoopHelper;

				CTestPerformance PerfTest(0.015);

				fs_MeasureSchemes
					(
						PerfTest
						, [&](CStr const &_Scheme)
						{
							fs_MeasureThroughput(PerfTest, RunLoopHelper, _Scheme, "Thr", 1);
						}
					)
				;
			};

			DMibTestSuite(CTestCategory("TransportThroughputConcurrent") << CTestGroup("Performance"))
			{
				CActorRunLoopTestHelper RunLoopHelper;

				CTestPerformance PerfTest(0.015);

				fs_MeasureSchemes
					(
						PerfTest
						, [&](CStr const &_Scheme)
						{
							int32 ConnectionConcurrency = fg_GetSys()->f_GetEnvironmentVariable("ConnectionConcurrency").f_ToInt(mc_nConcurrentConnections);
							fs_MeasureThroughput(PerfTest, RunLoopHelper, _Scheme, "ThrCon", ConnectionConcurrency);
						}
					)
				;
			};

			DMibTestSuite(CTestCategory("TransportRemoteServe") << CTestGroup("Manual"))
			{
				CStr Host = fg_GetSys()->f_GetEnvironmentVariable("BenchHost");
				if (!Host.f_IsEmpty())
				{
					CStr Scheme = fg_GetSys()->f_GetEnvironmentVariable("BenchSchemes");
					if (Scheme.f_IsEmpty())
						Scheme = "wss";
					uint32 Port = fg_GetSys()->f_GetEnvironmentVariable("BenchPort").f_ToInt(uint32(39301));
					CStr TicketFile = fg_GetSys()->f_GetEnvironmentVariable("BenchTicketFile");
					if (TicketFile.f_IsEmpty())
						TicketFile = NFile::CFile::fs_GetProgramDirectory() / "TransportBench.ticket";
					fp64 ServeSeconds = fg_GetSys()->f_GetEnvironmentVariable("BenchServeSeconds").f_ToFloat(fp64(600.0));

					CActorRunLoopTestHelper RunLoopHelper;

					CTrustManagerTestHelper ServerState;
					ServerState.m_DefaultSendWindowBytes = fg_BenchSendWindowBytes();
					TCActor<CDistributedActorTrustManager> ServerTrustManager = ServerState.f_TrustManager("Server");

					CDistributedActorTrustManager_Address ServerAddress;
					ServerAddress.m_URL = "{}://{}:{}/"_f << Scheme << Host << Port;
					ServerTrustManager(&CDistributedActorTrustManager::f_AddListen, ServerAddress, 0).f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout);

					TCUniquePointer<CDistributedActorTestHelper> pServerHelper = fg_Construct(ServerTrustManager, RunLoopHelper.m_pRunLoop);
					pServerHelper->f_Publish<CTransportBenchActor>
						(
							pServerHelper->f_GetManager()->f_ConstructActor<CTransportBenchActor>()
							, CTransportBenchActor::mc_pDefaultNamespace
						)
					;

					auto TrustTicket = ServerTrustManager(&CDistributedActorTrustManager::f_GenerateConnectionTicket, ServerAddress, nullptr, nullptr)
						.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
					;
					NFile::CFile::fs_WriteStringToFile(TicketFile, CStr(TrustTicket.m_Ticket.f_ToStringTicket()), false);

					NSys::fg_ConsoleOutput(CStr("TransportRemoteServe: {} for {} seconds, ticket at {}\n"_f << ServerAddress.m_URL << ServeSeconds << TicketFile));

					// Removing the ticket file ends the serve early, which is how a driver
					// script releases the server the moment its client is done
					NTime::CStopwatch Serving(true);
					while (Serving.f_GetTime() < ServeSeconds && NFile::CFile::fs_FileExists(TicketFile))
						NSys::fg_Thread_Sleep(0.25);

					ServerTrustManager->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());
				}
			};

			DMibTestSuite(CTestCategory("TransportRemoteThroughput") << CTestGroup("Manual"))
			{
				CStr TicketString = fg_GetSys()->f_GetEnvironmentVariable("BenchTicket");
				CStr TicketFile = fg_GetSys()->f_GetEnvironmentVariable("BenchTicketFile");
				if (TicketString.f_IsEmpty() && !TicketFile.f_IsEmpty())
					TicketString = NFile::CFile::fs_ReadStringFromFile(TicketFile);

				if (!TicketString.f_IsEmpty())
				{
					int32 ConnectionConcurrency = fg_GetSys()->f_GetEnvironmentVariable("ConnectionConcurrency").f_ToInt(mc_nConcurrentConnections);
					fp64 CallTimeout = fg_GetSys()->f_GetEnvironmentVariable("BenchCallTimeout").f_ToFloat(fp64(600.0));

					CActorRunLoopTestHelper RunLoopHelper;

					CTestPerformance PerfTest(0.015);

					auto Ticket = CDistributedActorTrustManager::CTrustTicket::fs_FromStringTicket(CStrSecure(TicketString));

					CTrustManagerTestHelper ClientState;
					ClientState.m_DefaultSendWindowBytes = fg_BenchSendWindowBytes();
					TCActor<CDistributedActorTrustManager> ClientTrustManager = ClientState.f_TrustManager("Client");
					ClientTrustManager(&CDistributedActorTrustManager::f_AddClientConnection, Ticket, g_Timeout / 2, ConnectionConcurrency, 0)
						.f_CallSync(RunLoopHelper.m_pRunLoop, g_Timeout)
					;

					TCUniquePointer<CDistributedActorTestHelper> pClientHelper = fg_Construct(ClientTrustManager, RunLoopHelper.m_pRunLoop);

					CStr Subscription = pClientHelper->f_Subscribe(CTransportBenchActor::mc_pDefaultNamespace);
					TCDistributedActor<CTransportBenchActor> RemoteActor = pClientHelper->f_GetRemoteActor<CTransportBenchActor>(Subscription);

					DMibTestPath("Remote");
					fs_MeasureThroughputOnActor(PerfTest, RunLoopHelper, RemoteActor, "ThrRemote", ConnectionConcurrency, CallTimeout);

					ClientTrustManager->f_BlockDestroy(RunLoopHelper.m_pRunLoop->f_ActorDestroyLoop());

					DMibExpectTrue(PerfTest);
				}
			};
		}

#if defined(DMibDebug) || defined(DMibSanitizerEnabled)
		constexpr static uint64 mc_nTransferBytes = 8ull << 20;
		constexpr static umint mc_nPingRoundTrips = 128;
		constexpr static umint mc_nRepetitions = 3;
#else
		constexpr static uint64 mc_nTransferBytes = 8 * (1024ull << 20);
		constexpr static umint mc_nPingRoundTrips = 4 * 16384;
		constexpr static umint mc_nRepetitions = 5;
#endif
		constexpr static uint32 mc_ChunkSize = NFile::gc_IdealIoSize;
		constexpr static int32 mc_nConcurrentConnections = 4;
	};

	DMibTestRegister(CDistributedActorTransportPerformance_Tests, Malterlib::Concurrency);
}
