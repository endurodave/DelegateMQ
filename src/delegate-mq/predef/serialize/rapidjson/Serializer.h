#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Serialize callable argument data using RapidJSON for transport
/// to a remote. Endinaness correctly handled by serialize class. 

#include "delegate/ISerializer.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
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
        try {
            os.seekp(0, std::ios::beg);
            rapidjson::StringBuffer sb;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

            (args.Write(writer, os), ...);  // C++17 fold expression to write each argument

            if (writer.IsComplete())
                os << sb.GetString();
            else
                os.setstate(std::ios::failbit);
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
            // Get stream length 
            std::streampos current_pos = is.tellg();
            is.seekg(0, std::ios::end);
            int length = static_cast<int>(is.tellg());
            is.seekg(current_pos, std::ios::beg);

            // Allocate storage buffer
            char* buf = (char*)malloc(length + 1);
            if (!buf)
                return is;

            // Copy into buffer 
            is.rdbuf()->sgetn(buf, length);
            buf[length] = 0;   // null terminate incoming data

            //std::cout << buf;

            // Parse JSON
            rapidjson::Document doc;
            doc.Parse(buf);
            free(buf);

            // Check for parsing errors
            if (doc.HasParseError())
            {
                // Let caller know read failed
                is.setstate(std::ios::failbit);
                std::cout << "Parse error: " << doc.GetParseError() << std::endl;
                std::cout << "Error offset: " << doc.GetErrorOffset() << std::endl;
                return is;
            }

            (args.Read(doc, is), ...);  // C++17 fold expression to read each argument
            return is;
        }
        catch (const std::exception& e) {
            std::cerr << "Deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif