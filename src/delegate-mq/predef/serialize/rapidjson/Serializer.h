#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// Serialize callable argument data using RapidJSON for transport
/// to a remote. Endinaness correctly handled by serialize class. 

#include "delegate/ISerializer.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include <iostream>

// make_serialized serializes each remote function argument
template<typename... Ts>
void make_serialized(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, std::ostream& os) { }

template<typename Arg1, typename... Args>
void make_serialized(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer, std::ostream& os, Arg1& arg1, Args... args) {
    arg1.Write(writer, os);
    make_serialized(writer, os, args...);
}

// make_unserialized unserializes each remote function argument
template<typename... Ts>
void make_unserialized(rapidjson::Document& doc, std::istream& is) { }

template<typename Arg1, typename... Args>
void make_unserialized(rapidjson::Document& doc, std::istream& is, Arg1& arg1, Args&&... args) {
    arg1.Read(doc, is);
    make_unserialized(doc, is, args...);
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data using serialize class
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public DelegateMQ::ISerializer<RetType(Args...)>
{
public:
    // Write arguments to a stream
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

        make_serialized(writer, os, args...);
        if (writer.IsComplete())
            os << sb.GetString();
        else
            os.setstate(std::ios::failbit);
        return os;
    }

    // Read arguments from a stream
    virtual std::istream& Read(std::istream& is, Args&... args) override {
        // Get stream length 
        std::streampos current_pos = is.tellg();
        is.seekg(0, std::ios::end);
        int length = static_cast<int>(is.tellg());
        is.seekg(current_pos, std::ios::beg);

        // Allocate storage buffer
        char* buf = (char*)malloc(length+1);
        if (!buf)
            return is;

        // Copy into buffer 
        is.rdbuf()->sgetn(buf, length);
        buf[length] = 0;   // null terminate incoming data

        //std::cout << buf;

        // Parse JSON
        rapidjson::Document doc;
        doc.Parse(buf);

        // Check for parsing errors
        if (doc.HasParseError()) 
        {
            // Let caller know read failed
            is.setstate(std::ios::failbit);
            std::cout << "Parse error: " << doc.GetParseError() << std::endl;
            std::cout << "Error offset: " << doc.GetErrorOffset() << std::endl;
            free(buf);
            return is;
        }

        make_unserialized(doc, is, args...);
        free(buf);
        return is;
    }
};

#endif