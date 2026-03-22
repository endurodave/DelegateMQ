# System Architecture Example (No Dependencies)

This example demonstrates a distributed system using `DelegateDataBus` for inter-process communication over UDP. It follows the pattern of a standalone Client and Server, similar to the `DelegateMQ` system-architecture example, but abstracted using the Data Bus.

## Architecture

- **Server (Publisher)**:
  - Initializes a `UdpTransport` in `PUB` mode.
  - Adds a `Participant` representing the Client.
  - Publishes `DataMsg` periodically to the topic `Topic/DataMsg`.
  - The Data Bus handles the serialization and network transmission automatically.

- **Client (Subscriber)**:
  - Initializes a `UdpTransport` in `SUB` mode.
  - Registers a `RemoteHandler` for `DataMsg`.
  - When a network message arrives, the handler republishes it to the **local** Data Bus.
  - Application components subscribe to the local topic name, remaining unaware that the data originated from a remote process.

## How to Run

1. **Start the Client**:
   ```bash
   ./build/Debug/DelegateDataBusClient.exe
   ```

2. **Start the Server**:
   ```bash
   ./build/Debug/DelegateDataBusServer.exe
   ```

You will see the Client receiving and printing the simulated sensor and actuator data from the Server.

## Key Features Shown

- **Location Transparency**: The client application code subscribes to a string topic, regardless of whether the data is local or remote.
- **UDP Distribution**: Using `UdpTransport` for point-to-point data delivery.
- **Custom Serialization**: Using `msg_serialize` for dependency-free binary serialization of complex structs and vectors.
