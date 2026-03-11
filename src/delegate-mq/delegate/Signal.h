#ifndef SIGNAL_H
#define SIGNAL_H

/// @file
/// @brief Delegate containers `Signal` that support RAII connection management.
///
/// @details This header defines `Signal` classes, which extend the standard
/// multicast delegates to return `Connection` handles upon subscription. These handles can be
/// wrapped in `ScopedConnection` to automatically unsubscribe when the handle goes out of scope.
///
/// @note `Signal` may be instantiated on the stack, as a class member, or on the heap.
/// A `Disconnect()` call after the `Signal` is destroyed is always a safe no-op.
/// For thread-safe signals where connections may be disconnected from other threads,
/// use `SignalSafe` managed by `std::shared_ptr` (see SignalSafe.h).

#include "MulticastDelegate.h"
#include <functional>
#include <memory>

namespace dmq {

    // --- Connection Handle Classes ---

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
        XALLOCATOR
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
        XALLOCATOR
    };

    // --- Signal Container ---

    template <class R>
    class Signal;

    /// @brief A non-thread-safe Multicast Delegate that returns a `Connection` handle.
    /// @details May be instantiated on the stack, as a class member, or on the heap.
    /// `Disconnect()` is always a safe no-op even if called after the Signal is destroyed.
    /// For concurrent multi-threaded disconnect, use `SignalSafe` instead.
    template<class RetType, class... Args>
    class Signal<RetType(Args...)>
        : public MulticastDelegate<RetType(Args...)>
    {
    public:
        using BaseType = MulticastDelegate<RetType(Args...)>;
        using DelegateType = Delegate<RetType(Args...)>;
        using MulticastDelegate<RetType(Args...)>::operator=;

        Signal() = default;
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;
        Signal(Signal&&) = delete;
        Signal& operator=(Signal&&) = delete;

        /// @brief Connect a delegate and return a unique handle.
        /// @details The returned `Connection` (or `ScopedConnection`) automatically
        /// removes the delegate when it goes out of scope or `Disconnect()` is called.
        /// Safe to call regardless of how the Signal was allocated.
        /// @return A `Connection` handle. Store in a `ScopedConnection` for automatic
        /// disconnect, or call `Disconnect()` manually.
        [[nodiscard]] Connection Connect(const DelegateType& delegate) {
            std::weak_ptr<void> weakLifetime = m_lifetime;

            this->PushBack(delegate);

            std::shared_ptr<DelegateType> delegateCopy(delegate.Clone());

            return Connection(weakLifetime, [this, weakLifetime, delegateCopy]() {
                // m_lifetime expires when Signal is destroyed, making this a safe no-op.
                if (!weakLifetime.expired()) {
                    this->Remove(*delegateCopy);
                }
            });
        }

        void operator+=(const DelegateType& delegate) {
            this->PushBack(delegate);
        }
        XALLOCATOR

    private:
        /// Lifetime sentinel: a heap-allocated token owned exclusively by this Signal.
        /// When Signal is destroyed, m_lifetime is destroyed and all weak_ptr copies
        /// held by disconnect lambdas expire, making subsequent Disconnect() calls no-ops.
        std::shared_ptr<void> m_lifetime{std::make_shared<bool>(true)};
    };

} // namespace dmq

#endif // SIGNAL_H
