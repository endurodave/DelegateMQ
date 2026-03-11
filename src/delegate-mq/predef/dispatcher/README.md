# Dispatcher Layer

This directory contains the dispatch logic for **DelegateMQ**, serving as the critical bridge between the high-level serialization layer and the low-level physical transport.

## Overview

The `Dispatcher` is responsible for taking the serialized payload (function arguments) and preparing it for transmission over the network or IPC link. It decouples the "what" (serialized data) from the "how" (transport mechanism).

## Key Components

* **`Dispatcher.h`**: The concrete implementation of the `IDispatcher` interface.
* **`RemoteChannel.h`**: An aggregator that owns a `Dispatcher`, an `xostringstream` (serialization buffer), and borrows an `ISerializer` for a single function signature. Use `RemoteChannel` to configure `DelegateMemberRemote` endpoints without manually wiring each component.

### Responsibilities

#### Dispatcher
1. **Protocol Framing**: It constructs the `DmqHeader` for every message, assigning the correct **Remote ID** and generating a monotonic **Sequence Number**.
2. **Stream Validation**: It ensures the output stream (`xostringstream`) contains valid data before transmission.
3. **Transport Handoff**: It forwards the framed message (Header + Payload) to the registered `ITransport` instance for physical transmission.

#### RemoteChannel
`RemoteChannel<Sig>` bundles the three objects a `DelegateMemberRemote` needs to send a message and exposes accessors so callers can wire them in one place:

```cpp
// Create one channel per message signature
RemoteChannel<void(AlarmMsg&, AlarmNote&)> alarmChannel(transport, alarmSerializer);

// Wire a delegate through the channel
myDelegate.Bind(this, &MyClass::Handler, ALARM_MSG_ID);
myDelegate.SetDispatcher(alarmChannel.GetDispatcher());
myDelegate.SetSerializer(alarmChannel.GetSerializer());
myDelegate.SetStream(&alarmChannel.GetStream());
```

The `MakeDelegate` overloads in `DelegateRemote.h` accept a `RemoteChannel` directly, making the wiring even more concise:

```cpp
auto d = MakeDelegate(this, &MyClass::Handler, ALARM_MSG_ID, alarmChannel);
```

