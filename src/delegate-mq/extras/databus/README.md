# DataBus

The `DataBus` is a central registry for topic-based communication within DelegateMQ. It provides a flexible publish-subscribe (Pub/Sub) architecture that decouples data producers from consumers.

## Features

- **Topic-Based Communication**: Components interact via named string topics rather than direct object references.
- **Thread Dispatching**: Subscribers can specify an `IThread` to have their callbacks executed on a specific thread.
- **Quality of Service (QoS)**: Supports Last Value Cache (LVC) to provide the most recent data to new subscribers immediately upon connection.
- **Filtering**: `SubscribeFilter` allows subscribers to receive only the data that matches a specific predicate.
- **Remote Distribution**: `Participant` integration allows the `DataBus` to span multiple physical nodes over any supported transport (UDP, TCP, ZeroMQ, etc.).
- **Monitoring & Spying**: The `Monitor` API allows for global observation of all bus traffic, useful for logging, debugging, or UI dashboards.
- **Type Safety**: Built on C++ templates to ensure type-safe data transmission.

## Basic Usage

### Subscribing to a Topic

```cpp
// Simple subscription on the current thread
auto conn = DataBus::Subscribe<int>("Temperature", [](int value) {
    std::cout << "Temp changed: " << value << std::endl;
});

// Subscription dispatched to a specific worker thread
auto conn2 = DataBus::Subscribe<int>("Temperature", &MyClass::OnTempChange, &workerThread);
```

### Publishing to a Topic

```cpp
DataBus::Publish<int>("Temperature", 25);
```

### Remote Participation

```cpp
// Add a remote participant (e.g., over UDP)
auto participant = std::make_shared<Participant>(transport);
DataBus::AddParticipant(participant);

// Register a serializer to allow local data to be sent to the remote participant
DataBus::RegisterSerializer<int>("Temperature", mySerializer);
```

## Internal Mechanics

The `DataBus` utilizes DelegateMQ's `MulticastDelegate` system internally. When you `Publish`, the bus identifies all local and remote subscribers for that topic and invokes them. Remote subscribers are handled via `IDispatcher` and `ITransport` layers, making the network boundary transparent to the application logic.
