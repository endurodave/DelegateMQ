#ifndef _ISERIALIZER_H
#define _ISERIALIZER_H

/// @file
/// @brief Delegate serializer interface class. 

#include <iostream>

namespace DelegateMQ {

template <class R>
struct ISerializer; // Not defined

/// @brief Delegate serializer interface for serializing and deserializing
/// remote delegate arguments. Implemented by application code if remote 
/// delegates are used.
/// 
/// @details All argument data is serialized into a stream. `write()` is called
/// by the sender when the delegate is invoked. `read()` is called by the receiver
/// upon reception of the remote message data bytes. 
template<class RetType, class... Args>
class ISerializer<RetType(Args...)>
{
public:
    /// Inheriting class implements the write function to serialize
    /// data for transport. 
    /// @param[out] os The output stream
    /// @param[in] args The target function arguments 
    /// @return The input stream
    virtual std::ostream& Write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& Read(std::istream& is, Args&... args) = 0;
};

}

#endif