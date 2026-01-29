# Dispatcher Layer

This directory contains the dispatch logic for **DelegateMQ**, serving as the critical bridge between the high-level serialization layer and the low-level physical transport.

## Overview

The `Dispatcher` is responsible for taking the serialized payload (function arguments) and preparing it for transmission over the network or IPC link. It decouples the "what" (serialized data) from the "how" (transport mechanism).

## Key Components

* **`Dispatcher.h`**: The concrete implementation of the `IDispatcher` interface.

### Responsibilities

1. **Protocol Framing**: It constructs the `DmqHeader` for every message, assigning the correct **Remote ID** and generating a monotonic **Sequence Number**.
2. **Stream Validation**: It ensures the output stream (`xostringstream`) contains valid data before transmission.
3. **Transport Handoff**: It forwards the framed message (Header + Payload) to the registered `ITransport` instance for physical transmission.

