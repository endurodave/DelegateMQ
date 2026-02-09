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

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data using serialize class
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    // Write arguments to a stream
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        os.seekp(0, std::ios::beg);
        serialize ser;
#if defined(__cpp_exceptions)
        try {
            (ser.write(os, args), ...);  // C++17 fold expression to serialize each argument
        }
        catch (const std::exception& e) {
            std::cerr << "Serialize error: " << e.what() << std::endl;
            throw;
        }
#else
        // STM32 / No Exceptions
        (ser.write(os, args), ...);
#endif
        return os;
    }

    // Read arguments from a stream
    virtual std::istream& Read(std::istream& is, Args&... args) override {
#if defined(__cpp_exceptions)
        try {
            serialize ser;
            (ser.read(is, args), ...);  // C++17 fold expression to unserialize each argument
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialize error: " << e.what() << std::endl;
            throw;
        }
#else
        // STM32 / No Exceptions
        serialize ser;
        (ser.read(is, args), ...);
#endif
        return is;
    }
};

#endif