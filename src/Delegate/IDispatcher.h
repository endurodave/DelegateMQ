#ifndef _IDISPATCHER_H
#define _IDISPATCHER_H

/// @file
/// @brief Delegate dispatcher interface class. 

namespace DelegateLib {

/// @brief Delegate interface class to dispatch serialized function argument data
/// to a remote destination. 
class IDispatcher
{
public:
    /// Dispatch a stream of bytes to a remote system. The implementer is responsible
    /// for sending the bytes over a communication link. 
    /// @param[in] os An outgoing stream to send to the remote destination.
    virtual int Dispatch(std::ostream& os) = 0;
};

}

#endif