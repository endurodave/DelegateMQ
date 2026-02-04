# Sample Projects

Remote delegates invoke a target function that runs in a separate processor or process. Transport and serialization libraries are configurable.

Each sample project focuses on a transport-serialization pair, but you can freely mix and match any transport with any serializer. 

## No External Dependencies

The following remote delegate sample projects have no external library dependencies. They rely only on the standard system APIs (Windows API, POSIX, etc.) or headers included within the repository.

* [bare-metal](./bare-metal/) - simple remote delegate app on Windows and Linux.
* [bare-metal-arm](./bare-metal-arm/) - ARM compiler example with no external libraries.
* [freertos-bare-metal](./freertos-bare-metal/) - FreeRTOS example on Windows (32-bit).
* [linux-tcp-serializer](./linux-tcp-serializer/) - simple TCP remote delegate app on Linux.
* [linux-udp-serializer](./linux-udp-serializer/) - simple UDP remote delegate app on Linux.
* [system-architecture-no-deps](./system-architecture-no-deps/) - complex remote delegate client/server apps using UDP on Windows or Linux.
* [win32-pipe-serializer](./win32-pipe-serializer/) - simple pipe remote delegate app on Windows.
* [win32-tcp-serializer](./win32-tcp-serializer/) - simple TCP remote delegate app on Windows.
* [win32-udp-serializer](./win32-udp-serializer/) - simple UDP remote delegate app on Windows.

## External Dependencies

The following projects require external 3rd party library support (e.g., STM32 Firmware, ZeroMQ, MQTT, RapidJSON, etc.). See [Examples Setup](../../docs/DETAILS.md#examples-setup) for external library installation setup.

* [mqtt-rapidjson](./mqtt-rapidjson/) - Remote delegate example with MQTT and RapidJSON.
* [nnq-bitsery](./nnq-bitsery/) - Remote delegate example using NNG and Bitsery libraries.
* [serialport-serializer](./serialport-serializer/) - Remote delegate example using libserialport.
* [stm32-freertos](./stm32-freertos/) - Simple FreeRTOS delegate app on STM32F4 Discovery board (Requires STM32Cube Firmware).
* [system-architecture](./system-architecture/) - System architecture example with dependencies.
* [system-architecture-python](./system-architecture-python/) - Python binding example.
* [zeromq-bitsery](./zeromq-bitsery/) - ZeroMQ transport with Bitsery serialization.
* [zeromq-cereal](./zeromq-cereal/) - ZeroMQ transport with Cereal serialization.
* [zeromq-msgpack-cpp](./zeromq-msgpack-cpp/) - ZeroMQ transport with MessagePack.
* [zeromq-rapidjson](./zeromq-rapidjson/) - ZeroMQ transport with RapidJSON.
* [zeromq-serializer](./zeromq-serializer/) - ZeroMQ transport with custom serializer.

## Build

See [Example Projects](../../docs/DETAILS.md#example-projects) for details on building and running remote delegate example projects.