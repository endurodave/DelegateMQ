# Cellutron Project Conventions

This document defines the coding standards and architectural patterns for the Cellutron project. All subsystems (Safety, Controller, GUI) must adhere to these rules to ensure consistency and reliable operation.

## 1. State Machine Pattern
The project uses a custom table-driven Finite State Machine (FSM) framework.

### Macro Usage
*   **Single-Line Declaration**: `BEGIN_TRANSITION_MAP` and `STATE_DEFINE` must be written on a single line to maintain readability and macro parser compatibility.
    *   *Correct*: `BEGIN_TRANSITION_MAP(cellutron::process::MyClass, MyFunc, data)`
    *   *Incorrect*: 
        ```cpp
        BEGIN_TRANSITION_MAP(cellutron::process::MyClass, 
           MyFunc, data)
        ```
*   **Fully Scoped Classes**: Always use the full namespace inside macros to avoid ambiguity.
    *   *Correct*: `STATE_DEFINE(cellutron::process::PumpProcess, Idle, NoEventData)`

### External Events Pattern
All external event methods (public methods that trigger a transition) must follow this pattern:
1.  Take exactly **one** argument: `std::shared_ptr<const T>` where `T` inherits from `EventData`.
2.  Use `BEGIN_TRANSITION_MAP` for automatic thread marshaling.
3.  Pass the shared pointer as the data argument in `END_TRANSITION_MAP`.
4.  **No Extra Code**: External event functions must contain **only** the transition map macros. Do not set variables or add logic, as these methods are not thread-safe. All logic must reside within the state actions (`ST_` methods).

### Asynchronous Signaling
When a state machine waits for an external signal (e.g., from hardware or another thread):
1.  **Store Configuration**: Store the current configuration (IDs, speeds, etc.) in a member variable (e.g., `m_data`) during the entry state.
2.  **Verify Feedback**: In the private signal handler (e.g., `OnValveChanged`), verify that the incoming hardware ID matches `m_data`.
3.  **Signal Handler Logic**: Signal handlers are **not** external events. They may contain `if/else` logic to evaluate the signal data and determine which semantic action to take.
4.  **Trigger Transitions**: Use the signal handler to call a dedicated **External Event** method (e.g., `ValveOpened()`) that uses `BEGIN_TRANSITION_MAP` to advance the state.
5.  **No `InternalEvent` in Handlers**: Do not use `InternalEvent` inside an asynchronous signal handler; it bypasses thread safety and can cause the machine to get stuck. Use declarative macro-driven event methods instead.

## 2. Naming & Placement Conventions
*   **Signals**: All `dmq::Signal` members must be prefixed with `On` (e.g., `OnComplete`, `OnValveChanged`, `OnTargetReached`).
*   **Signal Placement**: Place all `dmq::Signal` members at the very top of the class `public` section to make the subsystem's interface clear at a glance.
*   **Signal Connections**: `dmq::Signal::Connect()` is marked `[[nodiscard]]`. You **must** store the returned `dmq::ScopedConnection` in a member variable. 
    *   **RAII Lifetime**: Discarding the return value (e.g., via `(void)`) causes the connection to be destroyed immediately, resulting in no callbacks and silent system-wide logic failures. 
    *   **Always Store**: Use `dmq::ScopedConnection` or `std::map<T, dmq::ScopedConnection>` to manage subscriber lifetimes.

## 3. Namespace Architecture
*   **`cellutron::process`**: High-level process logic, state machines, and system coordinators.
*   **`cellutron::actuators`**: Low-level actuator abstractions (Valve, Pump, Centrifuge) and management.
*   **`cellutron::sensors`**: Low-level sensor abstractions and monitoring.
*   **Global**: `dmq::Thread`, `dmq::util::Timer`, and `dmq::FullPolicy` are classes from the DelegateMQ extras/ports. Always prefix them with `dmq::` or `dmq::util::`.
*   **DelegateMQ Inclusions**: Always include `DelegateMQ.h` as the primary entry point for the library. It automatically includes all necessary delegate types, port abstractions, and transport layers based on the build configuration. **Do not** include internal library files directly (e.g., avoid `#include "port/transport/win32-udp/Win32UdpTransport.h"`) to maintain platform portability.

## 4. Portability & Types
To ensure the system can run on FreeRTOS, Windows (Win32), and Standard C++ targets without code changes, use the provided portable abstractions instead of OS-specific or `std` primitives:
*   **Threading**: Use `dmq::Thread` (never `std::thread` or `xTaskCreate`).
*   **Sleep/Delay**: Use `dmq::Thread::Sleep()` (never `vTaskDelay`, `::Sleep()`, or `std::this_thread::sleep_for()`).
*   **Synchronization**: Use `dmq::Mutex` or `dmq::RecursiveMutex` (never `std::mutex` or `SemaphoreHandle_t`).
*   **Time**: Use `dmq::util::Timer`, `dmq::Duration`, and `dmq::TimePoint`.
*   **Fixed-Block Allocator**: Use the `XALLOCATOR` macro in class definitions to enable the optional fixed-block memory allocator.

## 5. Active Object & Threading
*   **Initialization**: Every application `main()` must instantiate `static NetworkContext networkContext;` at the very beginning to initialize the platform network stack (WinSock).
*   **Thread Marshaling**: Use `dmq::MakeDelegate` and the state machine's `SetThread()` capability to ensure all logic for a subsystem executes on a single dedicated thread.
*   **Watchdogs**: Every worker thread should be created with a watchdog timeout (typically 2 seconds) to allow the system to detect and handle deadlocks.

## 6. Communication & DataBus
*   **Serializers**: Every remote message type must be registered via `RegisterSerializers()` in `RemoteConfig.cpp` using the standardized `RID_` constants.
*   **Stringifiers**: Every topic must have a stringifier registered via `RegisterStringifiers()` to ensure `dmq-spy` and `dmq-monitor` can display human-readable data instead of "?".
*   **Topic Mapping**: Outgoing topics must be mapped to remote nodes using `Network::GetInstance().AddRemoteNode()` and the appropriate `RID_` identifiers.

## 7. Debugging & Tooling
*   **Spy & Monitor Tools**: Use the `dmq-spy` and `dmq-monitor` tools (found in `tools/build/Release`) to visualize DataBus traffic in real-time.
*   **Logging**: `dmq-spy` can be launched with `--log <filename>` to capture an audit trail of all messages. This is the primary method for post-run analysis.
*   **Console Output**: Use `printf` or `std::cout` for high-level state transition logging in console applications to provide immediate feedback during development.
