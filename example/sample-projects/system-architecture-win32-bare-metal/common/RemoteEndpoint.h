#ifndef REMOTE_ENDPOINT_H
#define REMOTE_ENDPOINT_H

#include "DelegateMQ.h"

template <class C, class R>
struct RemoteEndpoint; // Not defined

// Class to handle send/receive remote delegate messages. Class sets all RemoteDelegate
// base class interfaces such as stream and serializer.
template <class TClass, class RetType, class... Args>
class RemoteEndpoint<TClass, RetType(Args...)> : public dmq::DelegateMemberRemote<TClass, RetType(Args...)>
{
public:
    // Remote delegate type definitions
    using Func = RetType(Args...);
    using BaseType = dmq::DelegateMemberRemote<TClass, RetType(Args...)>;

    // Error handler callback
    dmq::MulticastDelegateSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCb;

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
    // Callback to catch remote delegate errors
    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        // Callback registered clients with error
        ErrorCb(id, error, aux);
    }

    // Serialize function argument data into a stream
    xostringstream m_argStream;

    // Remote delegate serializer
    Serializer<Func> m_msgSer;
};

#endif

