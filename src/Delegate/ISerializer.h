#ifndef _ISERIALIZER_H
#define _ISERIALIZER_H

/// @file
/// @brief Delegate serializer interface class. 

namespace DelegateLib {

template <class R>
struct ISerializer; // Not defined

/// @brief Delegate serializer interface for serializing and deserializing
/// remote delegate arguments. 
template<class RetType, class... Args>
class ISerializer<RetType(Args...)>
{
public:
    /// Inheriting class implements the write function to serialize
    /// data for transport. 
    /// @param[out] os The output stream
    /// @param[in] args The target function arguments 
    /// @return The input stream
    virtual std::ostream& write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& read(std::istream& is, Args&... args) = 0;
};

}

#endif