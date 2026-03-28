# DelegateMQ C# Interop

C# client for communicating with a C++ DelegateMQ DataBus server over UDP.

## Files

| File            | Purpose                                               |
|-----------------|-------------------------------------------------------|
| `DmqDataBus.cs`  | DataBus participant — UDP framing, ACKs, polling      |
| `Serializer.cs` | MessagePack encode/decode helpers                     |
| `Example.cs`    | Runnable example pairing with a C++ DataBus server    |

## Requirements

.NET 6 or later. Add the MessagePack NuGet package:

```
dotnet add package MessagePack
```

## Quick Start

```csharp
using DelegateMQ.Interop;

void OnData(ushort remoteId, byte[] payload)
{
    var fields = Serializer.Unpack(payload);
    Console.WriteLine($"Received {fields.Length} fields");
}

using var client = new DmqDataBus("127.0.0.1", recvPort: 8000, sendPort: 8001);
client.RegisterCallback(100, OnData);    // 100 = DataMsgId
client.Start();

// Send a CommandMsg (pollingRateMs = 500)
client.Send(101, Serializer.Pack(500));  // 101 = CommandMsgId

Thread.Sleep(30_000);
```

## Running the Example

Start the C++ DataBus server first (built with `DMQ_SERIALIZE_MSGPACK`), then:

```
dotnet run [duration_seconds]
```

## API

### `DmqDataBus(string remoteHost, int recvPort, int sendPort)`

- `remoteHost` — IP address of the C++ server
- `recvPort`   — local port to bind for incoming messages (C++ PUB port)
- `sendPort`   — remote port to send to (C++ SUB port)

### `client.RegisterCallback(ushort remoteId, Action<ushort, byte[]> callback)`

Register a callback for an incoming remote ID.
The callback is invoked on the background receive thread with `(remoteId, payloadBytes)`.

### `client.Send(ushort remoteId, byte[] payload)`

Send a MessagePack-serialized message. Use `Serializer.Pack()` to encode.

### `client.Start()` / `client.Stop()` / `client.Dispose()`

Open sockets and start the receive thread / stop and close sockets.
`DmqDataBus` implements `IDisposable` — use with `using`.

## Thread Safety

`RegisterCallback()` and `Send()` are safe to call from any thread.
Callbacks are invoked on the internal receive thread — keep them short or
dispatch to your own context if needed.
