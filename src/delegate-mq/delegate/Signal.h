#ifndef SIGNAL_H
#define SIGNAL_H

/// @file
/// @brief Delegate containers `Signal` that support RAII connection management.
///
/// @details This header defines `Signal` classes, which extend the standard
/// multicast delegates to return `Connection` handles upon subscription. These handles can be
/// wrapped in `ScopedConnection` to automatically unsubscribe when the handle goes out of scope.
///
/// @note Signals **MUST** be instantiated via `std::make_shared` (or `dmq::MakeSignal`). 
/// Instantiating them on the stack will cause a runtime crash (std::bad_weak_ptr) when 
/// `Connect()` is called.

#include "MulticastDelegate.h"
#include <functional>
#include <memory>
#include <cassert>

namespace dmq {

    // --- Connection Handle Classes ---
    // (Connection and ScopedConnection classes remain unchanged)

    /// @brief Represents a unique handle to a delegate connection. 
    /// Move-only to prevent double-disconnection bugs.
    class Connection {
    public:
        Connection() = default;

        template<typename DisconnectFunc>
        Connection(std::weak_ptr<void> watcher, DisconnectFunc&& func)
            : m_watcher(watcher)
            , m_disconnect(std::forward<DisconnectFunc>(func))
            , m_connected(true)
        {
        }

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        Connection(Connection&& other) noexcept
            : m_watcher(std::move(other.m_watcher))
            , m_disconnect(std::move(other.m_disconnect))
            , m_connected(other.m_connected)
        {
            other.m_connected = false;
            other.m_disconnect = nullptr;
        }

        Connection& operator=(Connection&& other) noexcept {
            if (this != &other) {
                Disconnect();
                m_watcher = std::move(other.m_watcher);
                m_disconnect = std::move(other.m_disconnect);
                m_connected = other.m_connected;
                other.m_connected = false;
                other.m_disconnect = nullptr;
            }
            return *this;
        }

        ~Connection() {}

        bool IsConnected() const {
            return m_connected && !m_watcher.expired();
        }

        void Disconnect() {
            if (!m_connected) return;
            if (!m_watcher.expired()) {
                if (m_disconnect) {
                    m_disconnect();
                }
            }
            m_disconnect = nullptr;
            m_watcher.reset();
            m_connected = false;
        }

    private:
        std::weak_ptr<void> m_watcher;
        std::function<void()> m_disconnect;
        bool m_connected = false;
    };

    /// @brief RAII wrapper for Connection. Automatically disconnects when it goes out of scope.
    class ScopedConnection {
    public:
        ScopedConnection() = default;
        ScopedConnection(Connection&& conn) : m_connection(std::move(conn)) {}
        ~ScopedConnection() { m_connection.Disconnect(); }

        ScopedConnection(ScopedConnection&& other) noexcept : m_connection(std::move(other.m_connection)) {}
        ScopedConnection& operator=(ScopedConnection&& other) noexcept {
            if (this != &other) {
                m_connection.Disconnect();
                m_connection = std::move(other.m_connection);
            }
            return *this;
        }

        ScopedConnection(const ScopedConnection&) = delete;
        ScopedConnection& operator=(const ScopedConnection&) = delete;

        void Disconnect() { m_connection.Disconnect(); }
        bool IsConnected() const { return m_connection.IsConnected(); }

    private:
        Connection m_connection;
    };

    // --- Signal Containers ---

    template <class R>
    class Signal;

    /// @brief A Multicast Delegate that returns a 'Connection' handle.
    /// @note Should be managed by std::shared_ptr to ensure thread-safe Disconnect.
    template<class RetType, class... Args>
    class Signal<RetType(Args...)>
        : public MulticastDelegate<RetType(Args...)>
        , public std::enable_shared_from_this<Signal<RetType(Args...)>>
    {
    public:
        using BaseType = MulticastDelegate<RetType(Args...)>;
        using DelegateType = Delegate<RetType(Args...)>;

        Signal() = default;
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;
        Signal(Signal&&) = delete;
        Signal& operator=(Signal&&) = delete;

        /// @brief Connect a delegate and return a unique handle.
        /// @details PRECONDITION: This Signal instance MUST be managed by a std::shared_ptr.
        [[nodiscard]] Connection Connect(const DelegateType& delegate) {
            std::weak_ptr<Signal> weakSelf;

            // Handle Assert vs Exception environments
#if defined(DMQ_ASSERTS) 
            // No exceptions: We simply assume the object is managed by shared_ptr.
            // If this object is on the stack, shared_from_this() will likely cause 
            // a strict abort/terminate depending on the STL implementation.
            weakSelf = this->shared_from_this();
#else
            try {
                weakSelf = this->shared_from_this();
            }
            catch (const std::bad_weak_ptr&) {
                assert(false && "Signal::Connect() requires the Signal instance to be managed by a std::shared_ptr. Use std::make_shared.");
                throw;
            }
#endif

            this->PushBack(delegate);

            std::shared_ptr<DelegateType> delegateCopy(delegate.Clone());

            return Connection(weakSelf, [weakSelf, delegateCopy]() {
                if (auto self = weakSelf.lock()) {
                    self->Remove(*delegateCopy);
                }
                });
        }

        void operator+=(const DelegateType& delegate) {
            this->PushBack(delegate);
        }
    };

} // namespace dmq

#endif // SIGNAL_H
