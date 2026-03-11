#ifndef REMOTE_CHANNEL_H
#define REMOTE_CHANNEL_H

/// @file RemoteChannel.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief Aggregates transport, serializer, dispatcher, and stream into a single
/// object, mirroring how `IThread`/`Thread` hides async delegate wiring.
///
/// @details
/// Setting up a remote delegate currently requires three separate objects:
/// @code
///   Dispatcher dispatcher;
///   dispatcher.SetTransport(&transport);
///   Serializer<void(int)> serializer;
///   xostringstream stream(std::ios::in | std::ios::out | std::ios::binary);
///
///   DelegateFreeRemote<void(int)> remote(REMOTE_ID);
///   remote.SetSerializer(&serializer);
///   remote.SetDispatcher(&dispatcher);
///   remote.SetStream(&stream);
/// @endcode
///
/// `RemoteChannel` collapses this into one setup object:
/// @code
///   MyTransport transport;
///   Serializer<void(int)> serializer;
///   RemoteChannel<void(int)> channel(transport, serializer);
///
///   // Mirrors the async pattern exactly:
///   auto async  = MakeDelegate(&MyFunc, thread);             // async
///   auto remote = MakeDelegate(&MyFunc, REMOTE_ID, channel); // remote
/// @endcode
///
/// @tparam Sig The function signature (e.g. `void(int, float)`) that matches both
/// the serializer and the remote delegates that use this channel. One channel
/// instance is required per distinct function signature.

#include "Dispatcher.h"
#include "delegate/DelegateRemote.h"
#include "predef/transport/ITransport.h"

namespace dmq {

template <class Sig>
class RemoteChannel; // Not defined

/// @brief Aggregates dispatcher, stream, and serializer for a single function signature.
///
/// @details `RemoteChannel` is non-copyable because it owns mutable stream state.
/// Delegates created via `MakeDelegate(..., channel)` borrow pointers to the
/// channel's internal dispatcher and stream, so the channel must outlive them.
///
/// @tparam RetType The return type of the remote function.
/// @tparam Args    The argument types of the remote function.
template <class RetType, class... Args>
class RemoteChannel<RetType(Args...)>
{
public:
    /// @brief Construct a RemoteChannel.
    /// @param[in] transport  The transport used to send serialized data. Caller owns it.
    /// @param[in] serializer The serializer matching the delegate signature. Caller owns it.
    RemoteChannel(ITransport& transport, ISerializer<RetType(Args...)>& serializer)
        : m_serializer(&serializer)
        , m_stream(std::ios::in | std::ios::out | std::ios::binary)
    {
        m_dispatcher.SetTransport(&transport);
    }

    // Non-copyable: owns stream state, and delegates hold raw pointers into this object.
    RemoteChannel(const RemoteChannel&) = delete;
    RemoteChannel& operator=(const RemoteChannel&) = delete;

    RemoteChannel(RemoteChannel&&) = default;
    RemoteChannel& operator=(RemoteChannel&&) = default;

    /// @brief Get the internal dispatcher (implements IDispatcher).
    IDispatcher* GetDispatcher() noexcept { return &m_dispatcher; }

    /// @brief Get the typed serializer supplied at construction.
    ISerializer<RetType(Args...)>* GetSerializer() noexcept { return m_serializer; }

    /// @brief Get the serialization output stream owned by this channel.
    xostringstream& GetStream() noexcept { return m_stream; }

private:
    Dispatcher m_dispatcher;
    xostringstream m_stream;
    ISerializer<RetType(Args...)>* m_serializer = nullptr;
};

/// @brief C++17 deduction guide — lets the compiler deduce `Sig` from the serializer type.
/// @details Enables `RemoteChannel channel(transport, serializer)` without an explicit
/// template argument.
template <class RetType, class... Args>
RemoteChannel(ITransport&, ISerializer<RetType(Args...)>&) -> RemoteChannel<RetType(Args...)>;

// ---- MakeDelegate overloads for RemoteChannel ----------------------------------
//
// These mirror the MakeDelegate(func, thread) async overloads exactly, but accept
// a RemoteChannel instead of an IThread, wiring up serializer, dispatcher, and
// stream in one step.

/// @brief Creates a remote delegate bound to a free function via a RemoteChannel.
/// @param[in] func    The free function to bind.
/// @param[in] id      The remote delegate identifier shared with the receiver.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateFreeRemote` ready to invoke remotely.
template <class RetType, class... Args>
auto MakeDelegate(RetType(*func)(Args...), DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateFreeRemote<RetType(Args...)> d(func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a non-const member function via a RemoteChannel.
/// @param[in] object  Raw pointer to the target object instance.
/// @param[in] func    The non-const member function to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateMemberRemote`.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(TClass* object, RetType(TClass::* func)(Args...), DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateMemberRemote<TClass, RetType(Args...)> d(object, func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a const member function via a RemoteChannel.
/// @param[in] object  Raw pointer to the target object instance.
/// @param[in] func    The const member function to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateMemberRemote`.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(TClass* object, RetType(TClass::* func)(Args...) const, DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateMemberRemote<TClass, RetType(Args...)> d(object, func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a const member function on a const object.
/// @param[in] object  Raw const pointer to the target object instance.
/// @param[in] func    The const member function to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateMemberRemote`.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(const TClass* object, RetType(TClass::* func)(Args...) const, DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateMemberRemote<const TClass, RetType(Args...)> d(object, func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a non-const member function via shared_ptr.
/// @param[in] object  Shared pointer to the target object instance.
/// @param[in] func    The non-const member function to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateMemberRemote`.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(std::shared_ptr<TClass> object, RetType(TClass::* func)(Args...), DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateMemberRemote<TClass, RetType(Args...)> d(object, func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a const member function via shared_ptr.
/// @param[in] object  Shared pointer to the target object instance.
/// @param[in] func    The const member function to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateMemberRemote`.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(std::shared_ptr<TClass> object, RetType(TClass::* func)(Args...) const, DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateMemberRemote<TClass, RetType(Args...)> d(object, func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

/// @brief Creates a remote delegate bound to a `std::function` via a RemoteChannel.
/// @param[in] func    The `std::function` to bind.
/// @param[in] id      The remote delegate identifier.
/// @param[in] channel The channel that provides dispatcher, serializer, and stream.
/// @return A fully configured `DelegateFunctionRemote`.
template <class RetType, class... Args>
auto MakeDelegate(std::function<RetType(Args...)> func, DelegateRemoteId id, RemoteChannel<RetType(Args...)>& channel)
{
    DelegateFunctionRemote<RetType(Args...)> d(func, id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

} // namespace dmq

#endif // REMOTE_CHANNEL_H
