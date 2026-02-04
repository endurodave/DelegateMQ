# DelegateMQ STM32F4 Discovery Example (FreeRTOS)

This project demonstrates the use of the **DelegateMQ** C++ library on embedded hardware using the **STM32F4 Discovery Board** (STM32F407VGT6). It runs on top of the STM32 HAL and showcases delegates, signals, and asynchronous messaging within a FreeRTOS environment.

## Overview

The application runs a suite of tests in `main.cpp` verifying the following DelegateMQ features on bare-metal/RTOS hardware:
* **Unicast Delegates:** Binding to free functions and lambdas.
* **Multicast Delegates:** Broadcasting events to multiple listeners.
* **Signals & Slots:** RAII-based connection management using `ScopedConnection`.
* **Thread Safety:** Using Mutex-protected delegates.
* **Async Dispatch:** Dispatching delegates across thread boundaries (if FreeRTOS is enabled).

## Prerequisites

* **Hardware:** STM32F4 Discovery Board (STM32F407G-DISC1).
* **IDE:** STM32CubeIDE (Tested on v2.0.0).
* **Firmware:** STM32Cube FW_F4 V1.28.3 (or compatible).
* **Library:** [DelegateMQ](https://github.com/endurodave/DelegateMQ) (Source code must be available locally).

## 🔧 Configuring Build Paths & Variables (Critical)

This project avoids hardcoded absolute paths by using Eclipse **Build Variables**. If you move this project to a new computer or update your STM32 Firmware version, you **must** update these variables, or the project will not build.

### Update the STM32 Firmware Path (`STM32_REPO`)

The project expects to find the STM32 HAL drivers via the `STM32_REPO` variable.

1.  Right-click the project in the Project Explorer -> **Properties**.
2.  Navigate to **C/C++ Build** > **Build Variables**.
3.  Find the variable named `STM32_REPO`.
4.  Click **Edit** and set it to your local STM32Cube repository path.
    * *Windows Example:* `C:\Users\YourName\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3`
    * *Linux/Mac Example:* `/home/yourname/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.3`
5.  Click **Apply**.

## 🔨 Building and Flashing

1. **Clean the Project:**
* Right-click the project -> **Clean Project**.


2. **Build:**
* Click the **Hammer** icon or press `Ctrl+B`.
* Ensure the console shows **"Build Finished"**.


3. **Flash:**
* Connect your Discovery board via USB (Mini-B).
* Click the **Debug** (Bug icon) or **Run** (Play icon).



## 💡 LED Status Indicators

* 🟠 **Orange (LED3):** System Initialization (main entered).
* 🟢 **Green (LED4):** FreeRTOS Scheduler Started & Test Task Running.
* 🔵 **Blue (LED6):** **SUCCESS** - Tests passed, application heartbeat blinking.
* 🔴 **Red (LED5):** **ERROR** - Initialization failed or Heap Exhausted.

## 📝 Configuring printf Output (SWV ITM)

This project redirects `printf` to the **SWV ITM Data Console** (Instrumentation Trace Macrocell). You must configure the debugger to capture this stream.

### 1. Enable SWV in Debug Configurations
1.  In STM32CubeIDE, click the arrow next to the **Debug** (Bug) icon and select **Debug Configurations...**.
2.  Select your project configuration on the left.
3.  Click the **Debugger** tab.
4.  Scroll down to the **Serial Wire Viewer (SWV)** section.
5.  **Check the "Enable" box.**
6.  **Set "Core Clock" to 168.0 MHz.**
    * *Note:* The STM32F4 Discovery runs at 168 MHz. If this value does not match the actual system clock, the output will be garbage or blank.
7.  Click **Apply** and then **Debug**.

### 2. Viewing the Output
Once the debug session starts (and the code is paused at `main`):
1.  Go to **Window** > **Show View** > **SWV** > **SWV ITM Data Console**.
2.  Click the **Configure Trace** button (Wrench/Gear icon 🔧) in the console toolbar.
3.  Under "ITM Stimulus Ports", **check Port 0**.
4.  Click **OK**.
5.  Click the **Start Trace** button (Red Circle 🔴) to begin recording.
6.  Press **Resume** (Play button) to run the code.
## Troubleshooting

* **Linker Error:** `undefined reference to xTimerCreate`
  * Ensure `#define configUSE_TIMERS 1` is set in `FreeRTOSConfig.h` and Rebuild (Clean first).


* **Blue LED never turns on (Application hangs)**
  * The Scheduler is likely not receiving ticks. Check `stm32f4xx_it.c` to ensure `SysTick_Handler` is calling `xPortSysTickHandler`.


* **Red LED Blinking Fast**
  * Heap Exhaustion. Increase `configTOTAL_HEAP_SIZE` in `FreeRTOSConfig.h`.


* **Error: exception handling disabled, use '-fexceptions'**
  * Ensure `DMQ_ASSERTS` is defined in project settings to disable try/catch blocks for embedded targets.