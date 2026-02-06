#ifndef SIGNAL_SAFE_H
#define SIGNAL_SAFE_H

/// @file
/// @brief Delegate container `SignalSafe` support RAII connection management.
///
/// @details This header defines `SignalSafe` classes, which extend the standard
/// multicast delegates to return `Connection` handles upon subscription. These handles can be
/// wrapped in `ScopedConnection` to automatically unsubscribe when the handle goes out of scope.
///
/// @note Signals **MUST** be instantiated via `std::make_shared` (or `dmq::MakeSignal`). 
/// Instantiating them on the stack will cause a runtime crash (std::bad_weak_ptr) when 
/// `Connect()` is called.

#include "Signal.h"
#include "MulticastDelegateSafe.h"
#include <functional>
#include <memory>
#include <cassert>

namespace dmq {
    template <class R>
    class SignalSafe;

    /// @brief A Thread-Safe Multicast Delegate that returns a 'Connection' handle.
    /// @note Should be managed by std::shared_ptr to ensure thread-safe Disconnect.
    template<class RetType, class... Args>
    class SignalSafe<RetType(Args...)>
        : public MulticastDelegateSafe<RetType(Args...)>
        , public std::enable_shared_from_this<SignalSafe<RetType(Args...)>>
    {
    public:
        using BaseType = MulticastDelegateSafe<RetType(Args...)>;
        using DelegateType = Delegate<RetType(Args...)>;
        using MulticastDelegateSafe<RetType(Args...)>::operator=;

        SignalSafe() = default;
        SignalSafe(const SignalSafe&) = delete;
        SignalSafe& operator=(const SignalSafe&) = delete;
        SignalSafe(SignalSafe&&) = delete;
        SignalSafe& operator=(SignalSafe&&) = delete;

        /// @brief Connect a delegate and return a unique handle.
        /// @details PRECONDITION: This SignalSafe instance MUST be managed by a std::shared_ptr.
        [[nodiscard]] Connection Connect(const DelegateType& delegate) {
            std::weak_ptr<SignalSafe> weakSelf;

            // Handle Assert vs Exception environments
#if !defined(__cpp_exceptions) || defined(DMQ_ASSERTS)
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

    // Alias for the shared_ptr type
    template<typename Signature>
    using SignalPtr = std::shared_ptr<SignalSafe<Signature>>;

    // Helper to create it easily
    template<typename Signature>
    SignalPtr<Signature> MakeSignal() {
        // If DMQ_ALLOCATOR is defined, DelegateBase::operator new is used.
        // We must use 'new' explicitly. std::make_shared would use the system heap.
        return std::shared_ptr<SignalSafe<Signature>>(new SignalSafe<Signature>());
    }

} // namespace dmq

#endif // _SIGNAL_SAFE_H