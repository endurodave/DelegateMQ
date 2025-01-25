#ifndef SERIALIZER_H
#define SERIALIZER_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// Example of a simple serializer relying upon insertion and extraction 
/// operators. 

#include <iostream>
#include "ISerializer.h"

// make_serialized serializes each remote function argument
template<typename... Ts>
void make_serialized(std::ostream& os) { }

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1& arg1, Args... args) {
    os << arg1;
    make_serialized(os, args...);
}

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1* arg1, Args... args) {
    os << *arg1;
    make_serialized(os, args...);
}

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1** arg1, Args... args) {
    static_assert(!std::is_pointer_v<Arg1>, "Pointer-to-pointer argument not supported");
}

// make_unserialized serializes each remote function argument
template<typename... Ts>
void make_unserialized(std::istream& is) { }

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1& arg1, Args... args) {
    is >> arg1;
    make_unserialized(is, args...);
}

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1* arg1, Args... args) {
    is >> *arg1;
    make_unserialized(is, args...);
}

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1** arg1, Args... args) {
    static_assert(!std::is_pointer_v<Arg1>, "Pointer-to-pointer argument not supported");
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public DelegateLib::ISerializer<RetType(Args...)>
{
public:
    virtual std::ostream& write(std::ostream& os, Args... args) override {
        make_serialized(os, args...);
        return os;
    }

    virtual std::istream& read(std::istream& is, Args&... args) override {
        make_unserialized(is, args...);
        return is;
    }
};

#endif