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
`RemoteChannel<Sig>` owns a `DelegateFunctionRemote` internally and handles all wiring automatically. Call `Bind()` once to configure the channel, then invoke it with `operator()`.

```cpp
// Create one channel per message signature
RemoteChannel<void(AlarmMsg&, AlarmNote&)> alarmChannel(transport, alarmSerializer);

// Bind a member function and remote ID — channel owns the delegate internally
alarmChannel.Bind(this, &MyClass::ForwardAlarm, ALARM_MSG_ID);

// Optionally register an error handler
alarmChannel.SetErrorHandler(MakeDelegate(this, &MyClass::OnError));

// Register the receive endpoint with NetworkEngine
RegisterEndpoint(ALARM_MSG_ID, alarmChannel.GetEndpoint());

// Invoke (fire-and-forget)
AlarmMsg msg; AlarmNote note;
alarmChannel(msg, note);

// Or invoke with blocking wait (via NetworkEngine::RemoteInvokeWait)
bool ok = RemoteInvokeWait(alarmChannel, msg, note);
```

Key accessors:
- `Bind(obj, func, id)` — binds a member function and configures all internal plumbing.
- `operator()(args...)` — invokes (sends) the remote message.
- `GetEndpoint()` — returns `IRemoteInvoker*` for `RegisterEndpoint()`.
- `GetError()` / `GetRemoteId()` — query last error and remote ID.
- `SetErrorHandler(delegate)` — register an error notification callback.

> **Legacy accessors** (`GetDispatcher()`, `GetSerializer()`, `GetStream()`) remain available for code that manually wires a `DelegateMemberRemote`.

