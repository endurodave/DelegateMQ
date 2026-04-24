# Sample Projects

## Feature & Toolchain Demos

The following projects demonstrate DelegateMQ delegate types (sync, async, asyncwait, multicast, signal) on specific platforms or compilers. No remote transport is involved.

* [atfe-armv7m-bare-metal](./atfe-armv7m-bare-metal/) - Arm Toolchain for Embedded (ATfE/Clang) bare-metal example for Armv7-M, runs on QEMU.
* [bare-metal-arm](./bare-metal-arm/) - ARM GCC bare-metal example, runs on QEMU.
* [clang-native](./clang-native/) - Comprehensive all-features demo using LLVM/Clang (or any C++17 compiler) natively on Windows or Linux.
* [keil-bare-metal](./keil-bare-metal/) - Keil MDK project on bare-metal ARM Cortex-M4.
* [stm32-freertos](./stm32-freertos/) - Simple FreeRTOS delegate app on STM32F4 Discovery board (Requires STM32Cube Firmware).

## Remote Delegate Examples

Remote delegates invoke a target function that runs in a separate processor or process. Transport and serialization libraries are configurable. Each sample project focuses on a transport-serialization pair, but you can freely mix and match any transport with any serializer.

### No External Dependencies

The following remote delegate projects have no external library dependencies. They rely only on the standard system APIs (Windows API, POSIX, etc.) or headers included within the repository.

* [bare-metal-remote](./bare-metal-remote/) - Simple remote delegate app on Windows and Linux.
* [databus](./databus/) - Distributed sensor/actuator system using `dmq::databus::DataBus` over UDP.
* [databus-freertos](./databus-freertos/) - FreeRTOS server (Win32 simulator, 32-bit) publishing sensor data to a Linux/Windows client over UDP, with the client sending rate-control commands back. Demonstrates mixed-platform DataBus with the FreeRTOS port.
* [databus-multicast](./databus-multicast/) - Distributed sensor/actuator system using `dmq::databus::DataBus` over UDP Multicast.
* [freertos-bare-metal](./freertos-bare-metal/) - FreeRTOS example on Windows (32-bit).
* [linux-tcp-serializer](./linux-tcp-serializer/) - Simple TCP remote delegate app on Linux.
* [linux-udp-serializer](./linux-udp-serializer/) - Simple UDP remote delegate app on Linux.
* [system-architecture-no-deps](./system-architecture-no-deps/) - Complex remote delegate client/server apps using UDP on Windows or Linux.
* [win32-pipe-serializer](./win32-pipe-serializer/) - Simple pipe remote delegate app on Windows.
* [win32-tcp-serializer](./win32-tcp-serializer/) - Simple TCP remote delegate app on Windows.
* [win32-udp-serializer](./win32-udp-serializer/) - Simple UDP remote delegate app on Windows.

### External Dependencies

The following projects require external 3rd party library support (e.g., ZeroMQ, MQTT, RapidJSON, etc.). See [Examples Setup](../../docs/DETAILS.md#examples-setup) for external library installation setup.

* [databus-interop](./databus-interop/) - Cross-language communication between C++ server and Python/C# clients using `dmq::databus::DataBus` and MessagePack.
* [databus-shapes](./databus-shapes/) - Graphical TUI Shapes Demo using `dmq::databus::DataBus`, UDP Multicast, and FTXUI.
* [mqtt-rapidjson](./mqtt-rapidjson/) - Remote delegate example with MQTT and RapidJSON.
* [nnq-bitsery](./nnq-bitsery/) - Remote delegate example using NNG and Bitsery libraries.
* [serialport-serializer](./serialport-serializer/) - Remote delegate example using libserialport.
* [system-architecture](./system-architecture/) - System architecture example with dependencies.
* [system-architecture-python](./system-architecture-python/) - Python binding example.
* [zeromq-bitsery](./zeromq-bitsery/) - ZeroMQ transport with Bitsery serialization.
* [zeromq-cereal](./zeromq-cereal/) - ZeroMQ transport with Cereal serialization.
* [zeromq-msgpack-cpp](./zeromq-msgpack-cpp/) - ZeroMQ transport with MessagePack.
* [zeromq-rapidjson](./zeromq-rapidjson/) - ZeroMQ transport with RapidJSON.
* [zeromq-serializer](./zeromq-serializer/) - ZeroMQ transport with custom serializer.

## Build

See [Example Projects](../../docs/DETAILS.md#example-projects) for details on building and running example projects.

### Building with Clang on Linux

All Linux and cross-platform projects that use `DMQ_THREAD_STDLIB` build cleanly with Clang. Pass the compiler at configure time:

```bash
cmake -S . -B build -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

This applies to: `bare-metal-remote`, `clang-native`, `databus`, `databus-multicast`, `linux-tcp-serializer`, `linux-udp-serializer`, `system-architecture-no-deps`, and the ZeroMQ-based projects (if ZeroMQ is installed). Windows-only and embedded projects are not applicable.
