#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// Example of serializer relying upon the serialize class to binary serialize
/// complex data objects including build-in and user defined data types.

#include "serialize.h"
#include <iostream>

// make_serialized serializes each remote function argument
template<typename... Ts>
void make_serialized(serialize& ser, std::ostream& os) { }

template<typename Arg1, typename... Args>
void make_serialized(serialize& ser, std::ostream& os, Arg1& arg1, Args... args) {
    ser.write(os, arg1);
    make_serialized(ser, os, args...);
}

// make_unserialized serializes each remote function argument
template<typename... Ts>
void make_unserialized(serialize& ser, std::istream& is) { }

template<typename Arg1, typename... Args>
void make_unserialized(serialize& ser, std::istream& is, Arg1& arg1, Args... args) {
    ser.read(is, arg1);
    make_unserialized(ser, is, args...);
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public ISerializer<RetType(Args...)>
{
public:
    virtual std::ostream& write(std::ostream& os, Args... args) override {
        make_serialized(m_ser, os, args...);
        return os;
    }

    virtual std::istream& read(std::istream& is, Args&... args) override {
        make_unserialized(m_ser, is, args...);
        return is;
    }

private:
    // The serialize class binary serializes data
    serialize m_ser;
};

#endif