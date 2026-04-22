# Cellutron Software Architecture

This document describes the software design and hardware topology of the Cellutron instrument.

---

## Hardware Topology

The system consists of three independent processing nodes (CPUs) communicating over a UDP-based distributed **DataBus**.

| CPU Node | Operating System | Primary Responsibility |
|:---|:---|:---|
| **GUI CPU** | Windows/Linux (stdlib) | HMI, Data Visualization, and System-wide Logging. |
| **Controller CPU** | FreeRTOS (Win32 Sim) | Process orchestration and hardware sequencing. |
| **Safety CPU** | FreeRTOS (Win32 Sim) | Independent hardware monitoring and interlock enforcement. |

---

## Thread Topology

Every CPU in the system uses a standardized thread architecture to handle network communication and DataBus (Active Object) dispatching. All threads (excluding the OS Main Thread) are monitored by the **DelegateMQ Watchdog** with a 2-second timeout to detect and handle deadlocks.

### GUI CPU
| Thread | Role | Description |
|:---|:---|:---|
| **Main Thread** | UI Event Loop | Handles FTXUI keyboard/mouse input and terminal rendering. |
| **NetworkThread** | UDP Receiver | Managed by the `Network` singleton. Listens for status updates from other CPUs. |
| **CommThread** | DataBus Callback | Dispatches incoming network topics into the UI state machine. |
| **LogThread** | File I/O | Dedicated thread for writing the `logs.txt` audit trail. |

### Controller CPU
| Thread | Role | Description |
|:---|:---|:---|
| **Main/Timer Thread** | OS Orchestration | Handles OS startup and DelegateMQ `Timer` processing. |
| **NetworkThread** | UDP Receiver | Managed by the `Network` singleton. Listens for commands from the GUI. |
| **CommThread** | Active Object SM | Standardized thread where the `CellProcess` and `Centrifuge` state machines execute. |
| **ActuatorThread** | Hardware Output | Dedicated thread for executing valve and pump commands via blocking sync calls. |
| **SensorThread** | Hardware Input | Dedicated thread for querying pressure and air-in-line sensors. |
| **Controller Thread** | Main FreeRTOS Loop | Primary application thread used for system setup and network initialization. |

### Safety CPU
| Thread | Role | Description |
|:---|:---|:---|
| **NetworkThread** | UDP Receiver | Managed by the `Network` singleton. Listens for real-time RPM updates. |
| **CommThread** | DataBus Callback | Standardized thread for running safety interlock and fault detection logic. |
| **Safety Thread** | Main FreeRTOS Loop | Primary application thread used for system setup and periodic checks. |

---

## Communication Model (Distributed DataBus)

Cellutron uses a "DDS-Lite" approach where data is exchanged via named **Topics**. The DataBus abstracts the network, allowing publishers and subscribers to interact seamlessly across CPU boundaries.

### Topic Mapping

| Topic | Publisher | Subscribers | Description |
|:---|:---|:---|:---|
| `Command` | GUI CPU | Controller CPU | Commands to start or abort the cell processing sequence. |
| `Status` | Controller CPU | GUI CPU | High-level system state (Idle, Processing, Aborting, Fault). |
| `Control` | Controller CPU | GUI CPU, Safety CPU | Real-time centrifuge RPM setpoints. |
| `Hardware`| Controller CPU | GUI CPU (logs) | Feedback on valve toggles, pump speed, and sensor snapshots. |
| `Fault` | Safety CPU | Controller CPU, GUI CPU | Critical safety violation event. |

---

## Subsystem Highlights

### 1. Synchronous Hardware Abstraction
The `Actuators` and `Sensors` subsystems demonstrate how DelegateMQ can provide a synchronous API for asynchronous hardware. By using delegates with return values, the `CellProcess` thread is **blocked** during a `SetValve()` call until the `ActuatorThread` confirms completion, ensuring deterministic sequencing without race conditions.

### 2. Traceability (logs)
The **logs** subsystem is a non-intrusive bus monitor. It subscribes to all `cmd/`, `status/`, and `hw/` topics to provide a timestamped audit trail in `logs.txt`. This allows for full reconstruction of a process run, including exactly when a valve opened relative to an RPM change.

### 3. Safety Interlocks
The **Safety CPU** implements an autonomous watchdog. It enforces hardware limits by publishing a `fault` topic that overrides the Controller's current state, illustrating independent safety layer design required for medical certifications.

---

## Implementation Details

- **Serialization**: Uses the `msg_serialize` port for compact binary transmission.
- **Shared Constants**: Centralized in `common/util/Constants.h`.
- **Async Marshalling**: Cross-thread and cross-processor logic is unified via DelegateMQ delegates.
