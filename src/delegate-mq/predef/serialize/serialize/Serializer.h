#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Serialize callable argument data using serialize class for transport
/// to a remote. Endinaness correctly handled by serialize class. 

#include "delegate/ISerializer.h"
#include "msg_serialize.h"
#include <iostream>

// make_serialized serializes each remote function argument
template<typename Arg1, typename... Args>
void make_serialized(serialize& ser, std::ostream& os, Arg1& arg1, Args... args) {
    ser.write(os, arg1);

    // Recursively call for other arguments
    if constexpr (sizeof...(args) > 0) {
        make_serialized(ser, os, args...);
    }
}

// make_unserialized unserializes each remote function argument
template<typename Arg1, typename... Args>
void make_unserialized(serialize& ser, std::istream& is, Arg1& arg1, Args&&... args) {
    ser.read(is, arg1);

    // Recursively call for other arguments
    if constexpr (sizeof...(args) > 0) {
        make_unserialized(ser, is, args...);
    }
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data using serialize class
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    // Write arguments to a stream
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        try {
            os.seekp(0, std::ios::beg);
            serialize ser;
            make_serialized(ser, os, args...);
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
            serialize ser;
            make_unserialized(ser, is, args...);
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif