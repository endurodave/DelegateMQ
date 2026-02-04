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

## ⚙️ Project Configuration

### 1. FreeRTOS Settings (`FreeRTOSConfig.h`)
To run DelegateMQ successfully, ensure the following settings are active:
* `#define configUSE_TIMERS 1` (Required for `Timer.cpp`)
* `#define configTOTAL_HEAP_SIZE ((size_t)(50 * 1024))` (Increased to 50KB for C++ objects)
* `#define xPortSysTickHandler SysTick_Handler` (Map FreeRTOS tick to CMSIS handler)

### 2. Interrupt Handling (`stm32f4xx_it.c`)
**Crucial:** The `SysTick_Handler` must "bridge" the HAL and the RTOS. Ensure it calls both:
```c
void SysTick_Handler(void) {
    HAL_IncTick(); // For STM32 HAL drivers
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler(); // For FreeRTOS Scheduler
    }
}
```

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

## 📝 Viewing Test Output (SWV)

Standard output (`printf`) is redirected to the **SWV ITM Data Console**.

1. Open **Window > Show View > SWV > SWV ITM Data Console**.
2. Click **Configure Trace** (Wrench icon) -> Check **Port 0**.
3. Click **Start Trace** (Red Circle).
4. Reset the board.

## Troubleshooting

* **Linker Error: `undefined reference to xTimerCreate**`
  * Ensure `#define configUSE_TIMERS 1` is set in `FreeRTOSConfig.h` and Rebuild (Clean first).


* **Blue LED never turns on (Application hangs)**
  * The Scheduler is likely not receiving ticks. Check `stm32f4xx_it.c` to ensure `SysTick_Handler` is calling `xPortSysTickHandler`.


* **Red LED Blinking Fast**
  * Heap Exhaustion. Increase `configTOTAL_HEAP_SIZE` in `FreeRTOSConfig.h`.


* **Error: exception handling disabled, use '-fexceptions'**
  * Ensure `DMQ_ASSERTS` is defined in project settings to disable try/catch blocks for embedded targets.