#ifndef _IDISPATCHER_H
#define _IDISPATCHER_H

/// @file
/// @brief Delegate dispatcher interface class. 

#include <cstdint>

namespace dmq {

// Remote identifier shared between sender and receiver remotes
typedef uint16_t DelegateRemoteId;
const uint16_t INVALID_REMOTE_ID = -1;

/// @brief Delegate interface class to dispatch serialized function argument data
/// to a remote destination. Implemented by the application if using remote delegates.
/// 
/// @details Incoming data from the remote must the `IDispatcher::Dispatch()` to 
/// invoke the target function using argument data. The argument data is serialized 
/// for transport using a concrete class implementing the `ISerialzer` interface 
/// allowing any data argument serialization method is supported.
/// @post The receiver calls `IRemoteInvoker::Invoke()` when the dispatched message
/// is received.
class IDispatcher
{
public:
    /// Dispatch a stream of bytes to a remote system. The implementer is responsible
    /// for sending the bytes over a communication transport (UDP, TCP, shared memory, 
    /// serial, ...). 
    /// @param[in] os An outgoing stream to send to the remote destination.
    /// @param[in] id The unique delegate identifier shared between sender and receiver.
    virtual int Dispatch(std::ostream& os, DelegateRemoteId id) = 0;
};

}

#endif