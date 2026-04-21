# Cellutron — Cell Processing System

**Cellutron** (located in `DelegateMQ/example/cellutron`) is a comprehensive demonstration project representing a hypothetical **medical, safety-critical instrument**. It showcases how **DelegateMQ** enables the design of distributed systems that require high reliability, independent hardware interlocks, and rigorous audit trails.

---

## Quick Start

1.  **Initialize Workspace**: From the repository root, run the setup scripts to fetch dependencies and build tools:
    ```powershell
    python 01_fetch_repos.py
    python 02_build_libs.py
    python 03_generate_samples.py
    python 04_build_samples.py
    python 05_run_samples.py
    ```
2.  **Build Cellutron**: From this directory (`example/cellutron/`), run:
    ```powershell
    cmake -B build .
    cmake --build build --config Debug
    ```
3.  **Run Cellutron**: Launch all three processors:
    ```powershell
    python run_cellutron.py
    ```

---

## Why Cellutron?

The Cellutron project serves as a "Real-World" demonstration of DelegateMQ in a multi-processor, safety-critical context. Unlike simple library examples, it showcases how DelegateMQ solves the challenges inherent in medical device engineering:

- **Safety Critical Isolation**: The system implements a dedicated **Safety CPU**. In medical devices, hardware interlocks must often be independent of the main control logic. Cellutron demonstrates an autonomous watchdog that can override the Controller if hardware limits are exceeded.
- **Heterogeneous Environments**: It runs identical application logic across diverse environments. The **GUI CPU** runs on a standard desktop OS, while the **Controller** and **Safety** nodes run on the **FreeRTOS Win32 Simulator**. This "sandbox" allows real RTOS kernel code and task-based logic to execute as standard Windows processes, providing a high-fidelity simulation of the microcontrollers found in modern instruments.
- **Distributed Triangle Monitoring**: Reliability is ensured via a "Triangle Monitoring" heartbeat architecture. Every node in the system (Controller, Safety, GUI) publishes a heartbeat and monitors the health of the other two. If any node stops responding (Loss of Signal), the entire system enters a non-recoverable safety fault.
- **Traceability and Audit Trails**: The **logs** subsystem demonstrates non-intrusive monitoring. It "spies" on the distributed bus to provide a timestamped audit trail of all commands, status changes, and raw hardware actions (valves/sensors)—a fundamental requirement for regulatory compliance (e.g., FDA/CE).
- **Concurrency without Locks**: Each processor uses the **Active Object** pattern. DelegateMQ's asynchronous delegates handle all data marshalling, allowing developers to write thread-safe state machines without the risk of deadlocks associated with manual mutex management.

---

## DelegateMQ Feature Showcase

Cellutron is designed to be the definitive reference for DelegateMQ, exercising all major functional areas of the library in a single integrated application.

### 1. Distributed DataBus (Local & Remote)
- **Many-to-Many**: Data flows seamlessly between three distributed CPUs and dozens of internal threads.
- **Location Transparency**: Publishers and Subscribers interact via named topics without knowing if the counterpart is in the same thread or across the network.
- **QoS (Last Value Cache)**: Critical state topics like `status/run` use LVC. If the GUI is restarted mid-process, it immediately receives the current instrument state from the bus.

### 2. Active Objects & Thread Marshalling
- **Dedicated Execution**: Every major subsystem (State Machines, Network, UI, Logs, Actuators, Sensors) is an independent **Active Object** owning its own thread.
- **Zero-Lock Concurrency**: DelegateMQ handles all inter-thread data marshalling. Developers write standard sequential code within state functions while the library ensures thread-safety without manual mutexes.

### 3. Synchronous-over-Asynchronous APIs
- **Blocking Hardware Abstraction**: The `actuators` and `sensors` subsystems return values (`int`), forcing the calling process thread to **block** until the hardware thread confirms completion. This demonstrates how to build a synchronous, easy-to-read API for asynchronous hardware interactions.

### 4. Signal & Slot (Multicast)
- **RAII Safety**: Uses `dmq::Signal` with `dmq::ScopedConnection` for internal events. This ensures that if a component is destroyed, its callbacks are automatically disconnected, preventing "latent" calls to dead objects and fixing `[[nodiscard]]` compiler warnings.
- **Event-Driven Design**: The `CellProcess` state machine is driven by asynchronous signals from the `Centrifuge` motor simulation.

### 5. Non-Intrusive Spying & Monitoring
- **Bus Monitoring**: The **logs** subsystem uses the `DataBus::Monitor()` feature to "spy" on every message flowing through the bus, providing a full audit trail without modifying a single line of code in the Controller or Safety CPUs.
- **Failure Detection**: Uses `SubscribeUnhandled()` to detect and log messages that were published but had no active subscribers.

### 6. Distributed Watchdog (Triangle Heartbeat)
- **Componentized Monitoring**: Uses a generalized `Heartbeat` utility class to manage cross-node health. 
- **Loss of Signal (LOS)**: Every node monitors its peers via `dmq::DeadlineSubscription`. If a heartbeat is missed for more than 2 seconds, a system-wide `FAULT` is triggered.
- **Warmup Protection**: Built-in logic ignores timeouts during the first 15 seconds of system boot-up to allow all nodes to synchronize.
- **Fault Storm Protection**: Logic ensures that even if multiple nodes detect a failure simultaneously, the system enters the safety state cleanly without redundant message flooding.

### 7. Multi-OS Portability
- **Heterogeneous Interop**: Identical application source code runs on **Standard C++ (GUI CPU)** and **FreeRTOS (Controller/Safety CPUs)**, showcasing the library's ability to abstract the underlying operating system and threading models.

---

## Architecture Overview

### Distributed Topology
![Cellutron Architecture](cellutron_architecture.svg)

The system is distributed across three independent processors connected via UDP.

| Processor | Role | OS / Port | Port |
|:---|:---|:---|:---|
| **GUI CPU** | Operator interface & Data logging | Windows/Linux | 5010 |
| **Controller CPU** | Main process state machines & Sequencing | FreeRTOS (Win32) | 5011 |
| **Safety CPU** | Independent monitor & Interlock verification | FreeRTOS (Win32) | 5013 |

### Pneumatics System
![Cellutron Pneumatics](pneumatics.svg)

The instrument controls a fluid path using a manifold of valves and a single peristaltic pump:
- **Valves V1-V3**: Divert fluids (Solution A, B, or Cells) into the main process line.
- **Pump (ID 1)**: Precise flow control across the fluid path.
- **Centrifuge**: High-speed separation chamber for cell processing.
- **Valve V10**: Controls the final drain path to waste.
- **Sensors**: Real-time pressure (P) and air-in-line (A) monitoring at the inlet and outlet of the pump manifold.

---

## Namespace Organization

The project uses a strict nested namespace architecture for clarity and to prevent collisions:
- `cellutron::process`: High-level process logic and state machines.
- `cellutron::actuators`: Hardware driver abstractions (Valve, Pump, Centrifuge).
- `cellutron::sensors`: Hardware sensor abstractions (Pressure, Air).
- `cellutron::hw`: Shared hardware data types.

---

## Communication Logic

- **Commands**: **GUI CPU** publishes `cmd/run` or `cmd/abort` to the bus. The **Controller CPU** subscribes and drives the state machine.
- **Control**: **Controller CPU** calculates the centrifuge ramp and publishes `cmd/speed` at high frequency.
- **Hardware Feedback**: The **actuators** and **sensors** subsystems on the Controller publish their internal states to `hw/status/actuator` and `hw/status/sensor`.
- **System Monitoring**: Both the **GUI CPU** (for UI/Logs) and **Safety CPU** (for interlocks) subscribe to the relevant status topics.
- **Safety Overrides**: If limits are exceeded or a heartbeat is lost, any node can publish a `fault` message. All nodes react immediately to enter a non-recoverable safe state.

---

For detailed hardware layout and flow paths, see [ARCHITECTURE.md](ARCHITECTURE.md).
