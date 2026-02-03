# DelegateMQ STM32F4 Discovery Example (FreeRTOS)

This project demonstrates the use of the **DelegateMQ** C++ library on embedded hardware using the **STM32F4 Discovery Board** (STM32F407VGT6). It runs on top of the STM32 HAL and showcases delegates, signals, and asynchronous messaging within a FreeRTOS environment.

## Overview

The application runs a suite of tests in `main.cpp` verifying the following DelegateMQ features on bare-metal/RTOS hardware:
* **Unicast Delegates:** Binding to free functions and lambdas.
* **Multicast Delegates:** Broadcasting events to multiple listeners.
* **Signals & Slots:**  RAII-based connection management using `ScopedConnection`.
* **Thread Safety:** Using Mutex-protected delegates.
* **Async Dispatch:** Dispatching delegates across thread boundaries (if FreeRTOS is enabled).

## Prerequisites

* **Hardware:** STM32F4 Discovery Board (STM32F407G-DISC1).
* **IDE:** STM32CubeIDE (Tested on v2.0.0).
* **Firmware:** STM32Cube FW_F4 V1.28.3 (or compatible).
* **Library:** [DelegateMQ](https://github.com/endurodave/DelegateMQ) (Source code must be available locally).

## ⚙️ Project Setup & Path Configuration

**CRITICAL STEP:** This project references external libraries (STM32 Drivers) using absolute paths. To build this on your machine, you must configure the **Build Variables** to point to your specific installation directories.

### 1. Configure the STM32 Firmware Repository Path
The compiler needs to know where your STM32 HAL and CMSIS drivers are located. We use a build variable named `STM32_REPO` to handle this.

1.  Open the project in **STM32CubeIDE**.
2.  Right-click the project `stm32-freertos` in the **Project Explorer** and select **Properties**.
3.  Navigate to **C/C++ Build** > **Environment**.
4.  Click **Add...** to create a new variable:
    * **Name:** `STM32_REPO`
    * **Value:** `[Path to your STM32 F4 Firmware]`
    * *Example Value:* `C:\Users\YourName\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.3`
5.  Click **OK**.
6.  Ensure the **"Add to environment"** checkbox is selected for `STM32_REPO`.
7.  Click **Apply and Close**.

### 2. Configure the DelegateMQ Library Path
The project expects the `delegate-mq` source code to be linked into the project tree.

1.  Look at the project explorer. If the folder `delegate-mq` has a small arrow icon but appears empty or has a red "x", the link is broken.
2.  **Delete** the broken `delegate-mq` folder from the Project Explorer (Right-click -> Delete -> OK). **Do not** delete contents on disk if asked.
3.  Right-click the project root -> **New** -> **Folder**.
4.  Click **Advanced >>**.
5.  Select **Link to alternate location (Linked Folder)**.
6.  Browse to the location where you downloaded the DelegateMQ library:
    * *Target:* `C:\Projects\DelegateMQWorkspace\DelegateMQ\src\delegate-mq` (or wherever you cloned the repo).
7.  Click **Finish**.

### 3. Verify Include Paths (Optional)
If the build fails after the steps above, ensure the compiler includes are using the variables:
1.  Go to **Properties** > **C/C++ Build** > **Settings**.
2.  Check **MCU GCC Compiler** > **Include paths**.
3.  Ensure you see paths utilizing the variable, such as:
    * `"${STM32_REPO}/Drivers/STM32F4xx_HAL_Driver/Inc"`
    * `"${workspace_loc:/${ProjName}/delegate-mq}"`

## 🔨 Building and Flashing

1.  **Clean the Project:**
    * Right-click the project -> **Clean Project**.
2.  **Build:**
    * Click the **Hammer** icon or press `Ctrl+B`.
    * Ensure the console shows **"Build Finished"**.
3.  **Flash:**
    * Connect your Discovery board via USB (Mini-B).
    * Click the **Debug** (Bug icon) or **Run** (Play icon).
    * Select **STM32 Cortex-M C/C++ Application** if prompted.


# Troubleshooting

* Error: fatal error: stm32f4xx_hal.h: No such file or directory

  * Your `STM32_REPO` variable is either not defined or pointing to the wrong version/folder. Check `Project Properties > C/C++ Build > Environment`.

* Error: exception handling disabled, use '-fexceptions'

  * Ensure `DMQ_ASSERTS` is defined in `DelegateOpt.h` or in your build settings. Embedded C++ usually runs with `-fno-exceptions`, so try/catch blocks must be compiled out using the library's preprocessor macros.

* Undefined reference to DelegateMQ symbols

  * Ensure the `delegate-mq` folder is correctly linked in the project explorer and that the source files inside it (if any .cpp exist) are not excluded from the build.