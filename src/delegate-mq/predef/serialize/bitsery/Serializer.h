#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
///
/// Serialize callable argument data using Bitsery for transport
/// to a remote. Bitsery provides fast, compact binary serialization.

#include "delegate/ISerializer.h"

// Core Bitsery
#include <bitsery/bitsery.h>
#include <bitsery/adapter/stream.h>

// Common Traits (Include these so standard types work out of the box)
#include <bitsery/traits/string.h>
#include <bitsery/traits/vector.h>
#include <bitsery/traits/list.h>

#include <sstream>
#include <iostream>
#include <type_traits>

template <class R>
struct Serializer; // Not defined

template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    // Bitsery Adapters for std::ostream / std::istream
    using OutputAdapter = bitsery::OutputStreamAdapter;
    using InputAdapter = bitsery::InputStreamAdapter;

    // Write: Changed 'Args... args' to 'const Args&... args' for efficiency
    virtual std::ostream& Write(std::ostream& os, const Args&... args) override {
        try {
            // Reset stream position.
            os.seekp(0, std::ios::beg);

            // Clear stringstreams explicitly to avoid appending new data to old data.
            // DelegateMQ often reuses the stream object.
            if (auto* ss = dynamic_cast<std::ostringstream*>(&os)) {
                ss->str("");
            }

            // Construct the adapter properly passing the stream
            bitsery::Serializer<OutputAdapter> writer{ OutputAdapter{os} };

            // Serialize each argument using C++17 fold expression
            (writer.object(args), ...);

            // Ensure buffer is flushed to the stream
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
            // Construct the adapter properly passing the stream
            bitsery::Deserializer<InputAdapter> reader{ InputAdapter{is} };

            // Deserialize each argument using fold expression
            (reader.object(args), ...);

            // Optional: Check for deserialization errors
            if (reader.adapter().error() != bitsery::ReaderError::NoError) {
                throw std::runtime_error("Bitsery reported a read error");
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Bitsery deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif // SERIALIZER_H