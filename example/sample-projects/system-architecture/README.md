The System Architecture example demonstrates a complex client-server DelegateMQ application. This example implements the acquisition of sensor and actuator data across two applications. It showcases communication and collaboration between subsystems, threads, and processes or processors. Delegate communication, callbacks, asynchronous APIs, and error handing are also highlighted. Notice how easily DelegateMQ transfers event data between threads and processes with minimal application code. The application layer is completely isolated from message passing details.

Follow the steps below to execute the projects.

1. Setup ZeroMQ and MessagePack external library dependencies. Alternatively, see the `system-architecture-bare-metal` directory project with no external dependencies.
2. Execute CMake command in `client` and `server` directories.  
   `cmake -B build .`
3. Build client and server applications within the `build` directory.
4. Start `delegate_server_app` first.
5. Start `delegate_client_app` second.
6. Client and server communicate and output debug data to the console.

