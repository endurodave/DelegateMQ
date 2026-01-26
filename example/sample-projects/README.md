# Sample Projects

Remote delegates invoke a target function that runs in a separate processor or process. Transport and serialization libraries are configurable.

Each sample project focuses on a transport-serialization pair, but you can freely mix and match any transport with any serializer. 

## External Dependencies

The following remote delegate sample projects have no external library dependencies:

* [bare-metal](./bare-metal/) - simple remote delegate app on Windows and Linux.
* [system-architecture-no-deps](./system-architecture-no-deps/) - complex remote delegate client/server apps using UDP on Windows or Linux.
* [linux-tcp-serializer](./linux-tcp-serializer/) - simple TCP remote delegate app on Linux.
* [linux-udp-serializer](./linux-udp-serializer/) - simple UDP remote delegate app on Linux.
* [win32-tcp-serializer](./win32-tcp-serializer/) - simple TCP remote delegate app on Windows.
* [win32-udp-serializer](./win32-udp-serializer/) - simple UDP remote delegate app on Windows.
* [win32-pipe-serializer](./win32-pipe-serializer/) - simple pipe remote delegate app on Windows.

All other projects require external 3rd party library support. See [Examples Setup](../../docs/DETAILS.md#examples-setup) for external library installation setup.

## Build

See [Example Projects](../../docs/DETAILS.md#example-projects) for details on building and running remote delegate example projects.