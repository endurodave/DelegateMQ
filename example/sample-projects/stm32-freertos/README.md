# DelegateMQ STM32F4 Discovery Example (FreeRTOS)

This project demonstrates the use of the **DelegateMQ** C++ library on embedded hardware using the **STM32F4 Discovery Board** (STM32F407VGT6). It runs on top of the STM32 HAL and showcases delegates, signals, and asynchronous messaging within a FreeRTOS environment.

## Overview

The application runs a suite of tests in `main.cpp` verifying the following DelegateMQ features on bare-metal/RTOS hardware:
* **Unicast Delegates:** Binding to free functions and lambdas.
* **Multicast Delegates:** Broadcasting events to multiple listeners.
* **Signals & Slots:** RAII-based connection management using `ScopedConnection`.
* **Thread Safety:** Using Mutex-protected delegates.
* **Async Dispatch:** Dispatching delegates across thread boundaries (if FreeRTOS is enabled).
* **Remote Messaging:** Serializing and transporting delegates over UART to a PC client.

## 🛠️ Hardware Prerequisites

### 1. Core Board (Required)
* **STM32F4 Discovery Board** (STM32F407G-DISC1)
    * This is the main MCU board where the FreeRTOS application runs.

### 2. Remote Communication (Required for Network Tests only)
To run the `RUN_NETWORK_TESTS` mode (Client-Server demo), you need the following additional hardware to provide an RS-232 serial interface:

* **STM32F4DIS-BB (Discovery Base Board)**
    * Provides the RS-232 DB9 connector connected to **USART6**.
* **Null Modem Cable** (DB9 Female to Female)
    * To connect the Base Board to your PC.
* **USB-to-Serial Adapter** (if your PC lacks a native RS-232 port).

> **Note:** If you only want to run the local `SIMPLE_TESTS` or `STRESS_TESTS` (internal loopback), the Base Board is **not** required.

## 💻 Software Prerequisites

* **IDE:** STM32CubeIDE (Tested on v2.0.0).
* **Firmware:** STM32Cube FW_F4 V1.28.3 (or compatible).
* **Library:** [DelegateMQ](https://github.com/endurodave/DelegateMQ) (Source code must be available locally).
* **PC Client:** The companion `ClientApp` (built via Visual Studio/CMake) running on Windows/Linux is required for Network Tests.

## ⚙️ Configuration

### 1. Build Paths (`STM32_REPO`)
This project uses Eclipse **Build Variables** to avoid hardcoded paths. You **must** configure this variable to point to your local STM32 firmware repository.

1.  Right-click the project -> **Properties**.
2.  Navigate to **C/C++ Build** > **Build Variables**.
3.  Edit `STM32_REPO` to match your installation:
    * *Windows Example:* `C:\Users\YourName\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3`
    * *Linux/Mac Example:* `/home/yourname/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3`
4.  Click **Apply**.

### 2. Selecting the Test Mode
Open `server/Src/main.cpp` and uncomment exactly **ONE** of the following lines to choose the operating mode:

```cpp
// ============================================================================
// TEST SELECTION (Uncomment ONE)
// ============================================================================
//#define RUN_SIMPLE_TESTS      // Runs local unit tests (delegates, timers)
//#define RUN_STRESS_TESTS      // Runs high-load async dispatch tests
#define RUN_NETWORK_TESTS     // Runs Remote Client-Server Demo (Requires RS-232)
```

## 🚀 Running Network Tests (Remote Demo)

This mode turns the STM32 into a **Server** that streams telemetry to a PC **Client**.

### 1. Hardware Setup
* Mount the Discovery Board onto the **STM32F4DIS-BB**.
* Connect the **RS-232 port** on the Base Board to your PC using a **Null Modem Cable**.
* Power the board (via USB Mini-B).

### 2. STM32 Setup
* Select `#define RUN_NETWORK_TESTS` in `main.cpp`.
* Build and Flash the board.
* **Verify:** The **Green LED (LD4)** should turn ON.

### 3. PC Client Setup
* Navigate to the `client` folder in this repository.
* Build the `ClientApp` using CMake or Visual Studio.
* Run the client executable.
* **Verify:** The client should print `Client start!` and begin displaying Sensor Data received from the STM32.

### 4. Success Indicator
* The **Blue LED (LD6)** on the STM32 will toggle every time a `DataMsg` packet is successfully sent to the PC.

## 💡 LED Status Indicators

* 🟠 **Orange (LED3):** System Initialization (main entered).
* 🟢 **Green (LED4):** FreeRTOS Scheduler Started & Test Task Running.
* 🔵 **Blue (LED6):** **Activity Heartbeat** (Toggles on successful data tx or timer tick).
* 🔴 **Red (LED5):** **ERROR** - Initialization failed, Heap Exhausted, or Assertion triggered.

## 📝 Configuring printf Output (SWV ITM)

This project redirects `printf` to the **SWV ITM Data Console**. To view debug logs:

1.  **Debug Configurations** > **Debugger** > **Serial Wire Viewer (SWV)**:
    * Check **Enable**.
    * Set **Core Clock** to **168.0 MHz**.
2.  Start Debugging.
3.  **Window** > **Show View** > **SWV** > **SWV ITM Data Console**.
4.  Click **Configure Trace** (🔧) -> Enable **Port 0**.
5.  Click **Start Trace** (🔴).

## Troubleshooting

### "ClientApp::Start() failed!" (PC Side)
* Ensure the Null Modem cable is connected.
* Ensure the STM32 is running `RUN_NETWORK_TESTS`.
* Check that the PC COM port matches the one configured in `ClientApp`.

### Blue LED never turns on
* The Scheduler is likely not receiving ticks. Check `stm32f4xx_it.c` to ensure `SysTick_Handler` calls `xPortSysTickHandler`.

### Red LED Blinking Fast
* Heap Exhaustion. Increase `configTOTAL_HEAP_SIZE` in `FreeRTOSConfig.h`.

### Linker Error: `undefined reference to xTimerCreate`
* Ensure `#define configUSE_TIMERS 1` is set in `FreeRTOSConfig.h`.