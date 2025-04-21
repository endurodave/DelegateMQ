#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
///
/// Serialize callable argument data using Bitsery for transport
/// to a remote. Bitsery provides fast, compact binary serialization.

#include "delegate/ISerializer.h"
#include <bitsery/bitsery.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/ext/std_tuple.h>
#include <sstream>
#include <iostream>
#include <type_traits>

// Type trait to check if a type is const
template <typename T>
using is_const_type = std::is_const<std::remove_reference_t<T>>;

template <class R>
struct Serializer; // Not defined

template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    using OutputAdapter = bitsery::OutputStreamAdapter;
    using InputAdapter = bitsery::InputStreamAdapter;

    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        try {
            os.seekp(0);
            bitsery::Serializer<OutputAdapter> writer{ os }; 

            // Serialize each argument using fold expression
            (writer.object(args), ...);
            writer.adapter().flush();
        }
        catch (const std::exception& e) {
            std::cerr << "Bitsery serialize error: " << e.what() << std::endl;
            throw;
        }
        return os;
    }

    virtual std::istream& Read(std::istream& is, Args&... args) override {
        try {
            bitsery::Deserializer<InputAdapter> reader{ is }; 

            // Deserialize each argument using fold expression
            (reader.object(args), ...);

        }
        catch (const std::exception& e) {
            std::cerr << "Bitsery deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif