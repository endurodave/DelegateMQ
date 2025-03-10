#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Serialize callable argument data using MessagePack library for transport
/// to a remote. Endinaness correctly handled by MessagePack library. 

#include "msgpack.hpp"
#include "delegate/ISerializer.h"
#include <iostream>
#include <type_traits>

// Type trait to check if a type is const
template <typename T>
using is_const_type = std::is_const<std::remove_reference_t<T>>;

// make_serialized serializes each remote function argument
template<typename Arg1, typename... Args>
void make_serialized(msgpack::sbuffer& buffer, Arg1& arg1, Args... args) {
    msgpack::pack(buffer, arg1);

    // Recursively call for other arguments
    if constexpr (sizeof...(args) > 0) {
        make_serialized(buffer, args...);
    }
}

// make_unserialized unserializes each remote function argument
template<typename Arg1, typename... Args>
void make_unserialized(msgpack::unpacker& unpacker, Arg1& arg1, Args&&... args) {
    static_assert(!is_const_type<Arg1>::value, "Arg1 cannot be const.");
    msgpack::object_handle oh;
    if (!unpacker.next(oh)) 
        throw std::runtime_error("Error during MsgPack unpacking.");
    arg1 = oh.get().as<Arg1>();

    // Recursively call for other arguments
    if constexpr (sizeof...(args) > 0) {
        make_unserialized(unpacker, args...);
    }
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data using MessagePack library
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    // Write arguments to a stream
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        try {
            msgpack::sbuffer buffer;
            make_serialized(buffer, args...);
            os.write(buffer.data(), buffer.size());
        }
        catch (const std::exception& e) {
            std::cerr << "Serialize error: " << e.what() << std::endl;
            throw;
        }
        return os;
    }

    // Read arguments from a stream
    virtual std::istream& Read(std::istream& is, Args&... args) override {
        try {
            std::string buffer_data((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
            msgpack::unpacker unpacker;
            unpacker.reserve_buffer(buffer_data.size());
            std::memcpy(unpacker.buffer(), buffer_data.data(), buffer_data.size());
            unpacker.buffer_consumed(buffer_data.size());
            make_unserialized(unpacker, args...);
        }
        catch (const msgpack::type_error& e) {
            std::cerr << "Deserialize type conversion error: " << e.what() << std::endl;
            throw;
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif