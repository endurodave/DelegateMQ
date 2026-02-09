#ifndef REMOTE_ENDPOINT_H
#define REMOTE_ENDPOINT_H

#include "DelegateMQ.h"

template <class C, class R>
struct RemoteEndpoint; // Not defined

/// @brief A helper class to handle the sending and receiving of remote delegate messages.
/// 
/// @details This class inherits from DelegateMemberRemote and encapsulates the necessary 
/// plumbing (Stream, Serializer, Dispatcher) required for remote communication. By 
/// internalizing these components, it simplifies the creation and registration of 
/// remote endpoints in the NetworkManager.
template <class TClass, class RetType, class... Args>
class RemoteEndpoint<TClass, RetType(Args...)> : public dmq::DelegateMemberRemote<TClass, RetType(Args...)>
{
public:
    // Remote delegate type definitions
    using Func = RetType(Args...);
    using BaseType = dmq::DelegateMemberRemote<TClass, RetType(Args...)>;
    using BaseType::operator=;

    // Clients connect to this signal to handle transport or serialization errors.
    dmq::SignalSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> OnError;

    // A remote delegate endpoint constructor
    RemoteEndpoint(dmq::DelegateRemoteId id, Dispatcher* dispatcher) :
        BaseType(id),
        m_argStream(std::ios::in | std::ios::out | std::ios::binary)
    {
        // Setup the remote delegate interfaces
        this->SetStream(&m_argStream);
        this->SetSerializer(&m_msgSer);
        this->SetDispatcher(dispatcher);
        this->SetErrorHandler(MakeDelegate(this, &RemoteEndpoint::ErrorHandler));
    }

private:
    // Callback to catch remote delegate errors (invoked by BaseType logic)
    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        // Emit the signal to registered clients
        OnError(id, error, aux);
    }

    // Serialize function argument data into a stream
    xostringstream m_argStream;

    // Remote delegate serializer
    Serializer<Func> m_msgSer;
};

#endif