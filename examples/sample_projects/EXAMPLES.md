# Remote Delegate Examples

Example projects showing how to use remote delegates to send and receive data between endpoints. All examples were built and testing using Visual Studio 2022. CMake build likely needs updates for other targets. The delegate library is cross-platform.

All remote delegate implementations require concrete implementations for `ISerializer` and `ITransport`. Interfaces classes provide hooks to customize the behavior of the delegate library.

# Bare Metal

The bare metal example shows how to use the a remote delegate without relying on any external libraries. See `bare_metal` directory.

| Interface | Interface Implementation 
| --- | ---
| `ISerializer` | Serialize argument data using insertion (`<<`) and extraction (`>>`) operators. 
| `ITransport` | Shared `std::stringstream` to simulate data send and receive.

# ZeroMQ

A [ZeroMQ](https://github.com/zeromq) integrated with remote delegates example. ZeroMQ is a high-performance, asynchronous messaging library that enables communication between distributed applications using various messaging patterns like pub-sub, request-reply, and push-pull. See `zero_mq` directory.

| Interface | Interface Implementation 
| --- | ---
| `ISerializer` | Serialize argument data using insertion and extraction operators. 
| `ITransport` | ZeroMQ used to send and receive data between two worker threads.

# ZeroMQ and serialize

A ZeroMQ and `serialize` integrated with remote delegates example. See `zero_mq_serializer` directory.

| Interface | Interface Implementation 
| --- | ---
| `ISerializer` | `serialize` class to serialize complex user defined argument data. 
| `ITransport` | ZeroMQ used to send and receive data between two worker threads.

