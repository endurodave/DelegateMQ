# DelegateMQ Interop

Cross-language client implementations for communicating with C++ DelegateMQ DataBus
applications over UDP. These clients implement the DmqHeader wire protocol and
MessagePack serialization — the same protocol spoken by the C++ DataBus library.

## Supported Languages

| Directory  | Language     | Transport | Serializer  |
|------------|--------------|-----------|-------------|
| `python/`  | Python 3.8+  | UDP       | MessagePack |
| `csharp/`  | C# / .NET 6+ | UDP       | MessagePack |

## Wire Protocol

All implementations speak the same 8-byte header protocol defined in
`src/delegate-mq/port/transport/DmqHeader.h`.

### DmqHeader (8 bytes, Network Byte Order / Big Endian)

| Offset | Size | Field   | Description             |
|--------|------|---------|-------------------------|
| 0      | 2    | Marker  | Always `0xAA55`         |
| 2      | 2    | ID      | DelegateRemoteId        |
| 4      | 2    | SeqNum  | Sequence number         |
| 6      | 2    | Length  | Payload length in bytes |

The header is immediately followed by `Length` bytes of MessagePack-serialized payload.

### ACK Packet

After receiving any non-ACK message, the receiver sends an ACK back to the sender:

- Header only (8 bytes, no payload)
- `ID = 0` (`ACK_REMOTE_ID` — reserved by DelegateMQ)
- `SeqNum` echoes the received sequence number
- `Length = 0`

### Serialization Format

Message fields are serialized as a **MessagePack array** in the same order as the
C++ `MSGPACK_DEFINE` macro:

```cpp
struct CommandMsg {
    int pollingRateMs = 1000;
    MSGPACK_DEFINE(pollingRateMs);
    // Wire: [1000]  — 1-element msgpack array
};

struct DataMsg {
    std::vector<Actuator> actuators;
    std::vector<Sensor>   sensors;
    MSGPACK_DEFINE(actuators, sensors);
    // Wire: [[[id,pos,v], ...], [[id,supV,readV], ...]]
};
```

## C++ Server Requirements

The C++ DataBus server must be configured with:

```cmake
set(DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")   # Windows
set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")   # Linux
```

## Port Convention

DataBus samples use two separate UDP ports — one for each direction of flow:

| Port | Direction        | Description                         |
|------|------------------|-------------------------------------|
| 8000 | C++ → Client     | C++ server publishes DataMsg here   |
| 8001 | Client → C++     | Client sends CommandMsg + ACKs here |

The client **binds** to port 8000 (receives) and **sends** to port 8001.
These must match the `UdpTransport::Create()` port arguments on the C++ side.

## Remote IDs

Remote IDs (`DelegateRemoteId`) are application-defined and must match between
the C++ server and the language client. They are typically declared in the C++
project's `SystemIds.h`:

```cpp
enum RemoteId : dmq::DelegateRemoteId {
    DataMsgId    = 100,
    CommandMsgId = 101,
};
```

`ID = 0` is reserved for ACK packets and must not be used by application messages.
