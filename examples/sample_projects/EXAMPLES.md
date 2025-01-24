# Remote Delegate Examples

Example projects showing how to use remote delegates to send and receive data between endpoints. All examples were built and testing using Visual Studio 2022. CMake build likely needs updates for other targets. The delegate library is cross-platform.

All remote delegate implementations require concrete implementations for `ISerializer` and `ITransport`. Interfaces classes provide hooks to customize the behavior of the delegate library.

# Bare Metal

The bare metal example shows how to use the a remote delegate without relying on any external libraries. See `bare_metal` directory.

| Interface | Interface Implementation |
| --- | --- |
| `ISerializer` | Serialize argument data using insertion (`<<`) and extraction (`>>`) operators. |
| `ITransport` | Shared `std::stringstream` to simulate data send and receive. |

# ZeroMQ

A [ZeroMQ](https://github.com/zeromq) integrated with remote delegates example. ZeroMQ is a high-performance, asynchronous messaging library that enables communication between distributed applications using various messaging patterns like pub-sub, request-reply, and push-pull. See `zeromq` directory.

| Interface | Interface Implementation |
| --- | --- |
| `ISerializer` | Serialize argument data using insertion (`<<`) and extraction (`>>`) operators. |
| `ITransport` | ZeroMQ used to send and receive data between two worker threads. |

# ZeroMQ and serialize

A ZeroMQ and `serialize` integrated with remote delegates example. Think of `serialize` a MessagePack-lite. See `zeromq_serializer` directory.

| Interface | Interface Implementation |
| --- | --- |
| `ISerializer` | `serialize` class to serialize complex user defined argument data. |
| `ITransport` | ZeroMQ used to send and receive data between two worker threads. |

# ZeroMQ and MessagePack

A ZeroMQ and [MessagePack](https://github.com/msgpack/msgpack-c/tree/cpp_master) integrated with remote delegates example. MessagePack is a binary serialization format that efficiently encodes data structures. See `zeromq_msgpack_cpp` directory.

| Interface | Interface Implementation |
| --- | --- |
| `ISerializer` | MsgPack used to serialize complex user defined argument data. |
| `ITransport` | ZeroMQ used to send and receive data between two worker threads. |

