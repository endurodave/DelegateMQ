# Serialization Layer

This directory contains the serialization adapters for **DelegateMQ**, responsible for marshaling function arguments into binary or text formats for remote transmission.

DelegateMQ is designed to be **serialization-agnostic**. You can choose the serializer that best fits your project's performance, size, or compatibility requirements.

## Supported Serializers

The following subdirectories contain adapters for popular C++ serialization libraries:

* **`serialize`**: The built-in, header-only **MessageSerialize** library. 
    * *Best for:* Zero dependencies, simple projects, and built-in STL container support.
    * *Features:* Endianness handling, versioning, and no external requirements.
* **`msgpack`**: Adapter for **MessagePack** (msgpack-c).
    * *Best for:* High-performance binary serialization and compact data size.
* **`rapidjson`**: Adapter for **RapidJSON**.
    * *Best for:* Human-readable JSON output and web API integration.
* **`cereal`**: Adapter for **Cereal**.
    * *Best for:* Modern C++11/17 features and robust object serialization.
* **`bitsery`**: Adapter for **Bitsery**.
    * *Best for:* Ultra-fast, zero-buffer serialization for real-time applications.

Alternatively, you can implement your own adapter by inheriting from the ISerializer interface and injecting it into your DelegateRemote instance.