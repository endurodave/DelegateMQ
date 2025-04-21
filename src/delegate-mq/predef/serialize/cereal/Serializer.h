#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Serialize callable argument data using Cereal library for transport
/// to a remote. Endianness handled internally by Cereal.

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "delegate/ISerializer.h"
#include <iostream>
#include <type_traits>
#include <sstream>

// Type trait to check if a type is const
template <typename T>
using is_const_type = std::is_const<std::remove_reference_t<T>>;

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data using Cereal
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    // Write arguments to a stream
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        try {
            os.seekp(0);
            cereal::BinaryOutputArchive archive(os);
            (archive(args), ...); // C++17 fold expression to serialize each argument
        }
        catch (const std::exception& e) {
            std::cerr << "Cereal serialize error: " << e.what() << std::endl;
            throw;
        }
        return os;
    }

    // Read arguments from a stream
    virtual std::istream& Read(std::istream& is, Args&... args) override {
        try {
            cereal::BinaryInputArchive archive(is);
            (archive(args), ...); // C++17 fold expression to deserialize
        }
        catch (const std::exception& e) {
            std::cerr << "Cereal deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif
