# Utility & Helper Layer

This directory contains concrete utility classes and helper functions that complement the core DelegateMQ library. 

While the core library (`namespace dmq`) provides the fundamental mechanism for delegates and threading interfaces, this directory provides the **"batteries included"** implementations required to build real-world applications (Timers, Reliability, Networking logic, etc.).

> **Note:** Code in this directory typically resides in the **global namespace**. These classes are designed to be used directly by your application or replaced with your own implementations.

## Functional Modules

### 1. Asynchronous Helpers
* **`AsyncInvoke.h`**: A powerful template helper that allows you to fire any function (lambda, member, free) onto any thread in a single line of code.
    * *Example:* `AsyncInvoke(myFunc, workerThread, 1s, arg1);`

### 2. Timing & Scheduling
* **`Timer.h`**: A complete software timer system capable of one-shot and periodic callbacks via delegates. Supports millisecond and microsecond precision depending on the platform clock.

### 3. Reliability Layer (QoS)
These classes provide transport-layer reliability (ACKs and Retries) over unreliable media (like UDP or Serial).
* **`TransportMonitor.h`**: Tracks outgoing messages and handles sequence numbers.
* **`RetryMonitor.h`**: Logic to detect lost packets and trigger re-transmissions.
* **`ReliableTransport.h`**: A composite transport that wraps a raw transport (e.g., UDP) and adds reliability logic transparently.

### 4. Networking Logic
* **`NetworkEngine.h`**: A high-level manager that coordinates the `Dispatcher` and `Transport` to simplify sending messages to remote endpoints.
* **`NetworkConnect.h`**: Platform-specific socket initialization helpers (e.g., `WinsockConnect`, `UnixConnect`) to handle networking boilerplate like `WSAStartup`.
* **`RemoteEndpoint.h`**: Base class for `DelegateMemberRemote` used to register receive-side endpoints with `NetworkEngine`.

### 5. System Utilities
* **`Fault.h`**: Assertions and fault trapping macros (`ASSERT_TRUE`, `FAULT_Handler`) used throughout the library examples.
* **`crc16.h`**: Checksum utility for data integrity in serial/UDP headers.

## Usage

Include these headers when you need specific functionality beyond basic delegates. For example, if you need to schedule a periodic callback:

```cpp
#include "extras/util/Timer.h"

Timer myTimer;
// Use a delegate to receive timer callbacks
myTimer.OnExpired += MakeDelegate(&myClass, &MyClass::OnTimeout);
myTimer.Start(1000); // 1-second periodic timer
```

Or to invoke a function on a specific thread with a 5-second timeout:

```cpp
#include "extras/util/AsyncInvoke.h"

// Fires and waits up to 5s for completion on workerThread
AsyncInvoke(myWorkerFunc, workerThread, 5s, 123, "hello");
```
