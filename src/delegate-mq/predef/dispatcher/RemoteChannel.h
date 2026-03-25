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

/// @brief Aggregates dispatcher, stream, serializer, and delegate binding for a single
/// function signature. The canonical way to configure a remote endpoint.
///
/// @details `RemoteChannel` is the single object a user needs to declare per message
/// signature. It owns the `Dispatcher`, the serialization stream, and the internal
/// `DelegateFunctionRemote` that handles both sending and receiving.
///
/// **Usage pattern (preferred):**
/// @code
///   // Declare per signature — one channel replaces channel + separate delegate
///   std::optional<RemoteChannel<void(AlarmMsg&)>> m_alarmChannel;
///
///   // In Create():
///   m_alarmChannel.emplace(GetSendTransport(), m_alarmSer);
///   m_alarmChannel->Bind(this, &MyClass::OnAlarm, ALARM_ID);
///   m_alarmChannel->SetErrorHandler(MakeDelegate(this, &MyClass::OnError));
///   RegisterEndpoint(ALARM_ID, m_alarmChannel->GetEndpoint());
///
///   // Send (fire-and-forget):
///   (*m_alarmChannel)(msg);
///
///   // Send (blocking wait):
///   RemoteInvokeWait(*m_alarmChannel, msg);
/// @endcode
///
/// **Legacy MakeDelegate pattern (still supported):**
/// @code
///   auto d = MakeDelegate(&MyFunc, REMOTE_ID, channel);
/// @endcode
///
/// `RemoteChannel` is non-copyable because it owns mutable stream state and the internal
/// delegate holds raw pointers into the channel.
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
        m_delegate.SetDispatcher(&m_dispatcher);
        m_delegate.SetSerializer(m_serializer);
        m_delegate.SetStream(&m_stream);
    }

    // Non-copyable: owns stream state and internal delegate holds raw pointers into this object.
    RemoteChannel(const RemoteChannel&) = delete;
    RemoteChannel& operator=(const RemoteChannel&) = delete;
    RemoteChannel(RemoteChannel&&) = delete;
    RemoteChannel& operator=(RemoteChannel&&) = delete;

    // -----------------------------------------------------------------------
    // Delegate binding — preferred user-facing API (Item 2 / Item 4)
    // -----------------------------------------------------------------------

    /// @brief Bind a non-const member function as the receive-side handler.
    /// @details Wires the internal delegate (dispatcher, serializer, stream) and sets
    /// the target function in one call. Call this once in your Create()/Initialize().
    /// @param[in] object  Raw pointer to the target object.
    /// @param[in] func    The non-const member function to call on receive.
    /// @param[in] id      The remote delegate identifier shared with the sender.
    template<class TClass>
    void Bind(TClass* object, RetType(TClass::* func)(Args...), DelegateRemoteId id) {
        m_delegate.Bind([object, func](Args... args) -> RetType {
            return (object->*func)(args...);
        }, id);
        m_delegate.SetDispatcher(&m_dispatcher);
        m_delegate.SetSerializer(m_serializer);
        m_delegate.SetStream(&m_stream);
    }

    /// @brief Bind a const member function as the receive-side handler.
    template<class TClass>
    void Bind(const TClass* object, RetType(TClass::* func)(Args...) const, DelegateRemoteId id) {
        m_delegate.Bind([object, func](Args... args) -> RetType {
            return (object->*func)(args...);
        }, id);
        m_delegate.SetDispatcher(&m_dispatcher);
        m_delegate.SetSerializer(m_serializer);
        m_delegate.SetStream(&m_stream);
    }

    /// @brief Bind a `std::function` as the receive-side handler.
    void Bind(std::function<RetType(Args...)> func, DelegateRemoteId id) {
        m_delegate.Bind(func, id);
        m_delegate.SetDispatcher(&m_dispatcher);
        m_delegate.SetSerializer(m_serializer);
        m_delegate.SetStream(&m_stream);
    }

    /// @brief Bind a raw lambda or functor as the receive-side handler.
    template <typename F, typename = std::enable_if_t<trait::is_callable<F>::value>>
    void Bind(F&& func, DelegateRemoteId id) {
        m_delegate.Bind(std::forward<F>(func), id);
        m_delegate.SetDispatcher(&m_dispatcher);
        m_delegate.SetSerializer(m_serializer);
        m_delegate.SetStream(&m_stream);
    }

    /// @brief Invoke the channel (fire-and-forget send).
    /// @pre Bind() must have been called first.
    void operator()(Args... args) { m_delegate(args...); }

    /// @brief Register an error handler delegate.
    template<class Handler>
    void SetErrorHandler(Handler&& handler) {
        m_delegate.SetErrorHandler(std::forward<Handler>(handler));
    }

    /// @brief Set the remote ID for outbound messages.
    /// @param[in] id The remote delegate identifier.
    void SetRemoteId(DelegateRemoteId id) noexcept { m_delegate.SetRemoteId(id); }

    /// @brief The remote ID set by the most recent Bind() or SetRemoteId() call.
    DelegateRemoteId GetRemoteId() noexcept { return m_delegate.GetRemoteId(); }

    /// @brief The error status of the most recent invocation.
    DelegateError GetError() noexcept { return m_delegate.GetError(); }

    /// @brief Returns the internal delegate as an IRemoteInvoker* for RegisterEndpoint().
    IRemoteInvoker* GetEndpoint() noexcept { return &m_delegate; }

    // -----------------------------------------------------------------------
    // Internal accessors — used by the MakeDelegate free-function overloads
    // defined below. Not intended for direct use by application code.
    // -----------------------------------------------------------------------

    /// @internal Used by MakeDelegate overloads. Prefer Bind() in application code.
    IDispatcher* GetDispatcher() noexcept { return &m_dispatcher; }

    /// @internal Used by MakeDelegate overloads. Prefer Bind() in application code.
    ISerializer<RetType(Args...)>* GetSerializer() noexcept { return m_serializer; }

    /// @internal Used by MakeDelegate overloads. Prefer Bind() in application code.
    xostringstream& GetStream() noexcept { return m_stream; }

private:
    Dispatcher m_dispatcher;
    xostringstream m_stream;
    ISerializer<RetType(Args...)>* m_serializer = nullptr;
    DelegateFunctionRemote<RetType(Args...)> m_delegate;
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

/// @brief Creates a remote delegate bound to a raw lambda or functor via a RemoteChannel.
template <typename F, typename Sig, typename = std::enable_if_t<trait::is_callable<F>::value>>
auto MakeDelegate(F&& func, DelegateRemoteId id, RemoteChannel<Sig>& channel)
{
    DelegateFunctionRemote<Sig> d(std::forward<F>(func), id);
    d.SetDispatcher(channel.GetDispatcher());
    d.SetSerializer(channel.GetSerializer());
    d.SetStream(&channel.GetStream());
    return d;
}

} // namespace dmq

#endif // REMOTE_CHANNEL_H
