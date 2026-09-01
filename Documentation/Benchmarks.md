# Benchmarks

## Remote distributed transport benchmark

The server publishes a download actor and writes a connection ticket. Copy the ticket to the client host.

Server settings:

```text
BenchHost=<routable address>
BenchPort=39301
BenchSchemes=wss
BenchTicketFile=<ticket path>
BenchServeSeconds=600
```

Only BenchHost is required. Client settings:

```text
BenchTicketFile=<copied ticket path>
# Or BenchTicket=<ticket string>
BenchCallTimeout=600
```

TransferBytes, ChunkSize, PipelineLength, ConnectionConcurrency and BenchStorage also apply to the client.
Missing required settings skip the manual suite. Removing the server ticket file stops the server early.
