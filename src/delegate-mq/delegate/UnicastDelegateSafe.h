#ifndef _UNICAST_DELEGATE_SAFE_H
#define _UNICAST_DELEGATE_SAFE_H

/// @file
/// @brief Delegate container for storing an invoking a single delegate instance. 
/// Class is thread-safe.

#include "UnicastDelegate.h"
#include <mutex>

namespace dmq {

template <class R>
struct UnicastDelegateSafe; // Not defined

/// @brief A thread-safe delegate container storing one delegate. Void and  
/// non-void return values supported. 
template<class RetType, class... Args>
class UnicastDelegateSafe<RetType(Args...)> : public UnicastDelegate<RetType(Args...)>
{
public:
    using DelegateType = Delegate<RetType(Args...)>;
    using BaseType = UnicastDelegate<RetType(Args...)>;

    UnicastDelegateSafe() = default;
    ~UnicastDelegateSafe() = default;

    UnicastDelegateSafe(const UnicastDelegateSafe& rhs) : BaseType() {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(rhs);
    }

    UnicastDelegateSafe(UnicastDelegateSafe&& rhs) : BaseType() {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(std::move(rhs));
    }

    /// Invoke the bound target.
    /// @param[in] args The arguments used when invoking the target function
    /// @return The target function return value. 
    RetType operator()(Args... args) {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator ()(args...);
    }

    /// Invoke the bound target functions. 
    /// @param[in] args The arguments used when invoking the target function
    void Broadcast(Args... args) {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::Broadcast(args...);
    }

    /// Assign a delegate to the container.
    /// @param[in] rhs A delegate target to assign
    void operator=(const DelegateType& rhs) {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(rhs);
    }

    /// Assign a delegate to the container.
    /// @param[in] rhs A delegate target to assign
    void operator=(DelegateType&& rhs) {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(rhs);
    }

    /// @brief Assignment operator that assigns the state of one object to another.
    /// @param[in] rhs The object whose state is to be assigned to the current object.
    /// @return A reference to the current object.
    UnicastDelegateSafe& operator=(const UnicastDelegateSafe& rhs) {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(rhs);
        return *this;
    }

    /// @brief Move assignment operator that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    /// @return A reference to the current object.
    UnicastDelegateSafe& operator=(UnicastDelegateSafe&& rhs) noexcept {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::operator=(std::move(rhs));
        return *this;
    }

    /// @brief Clear the all target functions.
    virtual void operator=(std::nullptr_t) noexcept { 
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::Clear();
    }

    /// Any registered delegates?
    /// @return `true` if delegate container is empty.
    bool Empty() { 
        const std::lock_guard<std::mutex> lock(m_lock);
        return BaseType::Empty();
    }

    /// Remove the registered delegate
    void Clear() {
        const std::lock_guard<std::mutex> lock(m_lock);
        BaseType::Clear();
    }

    /// Get the number of delegates stored.
    /// @return The number of delegates stored.
    std::size_t Size() { 
        const std::lock_guard<std::mutex> lock(m_lock);
        return BaseType::Size();
    }

    /// @brief Implicit conversion operator to `bool`.
    /// @return `true` if the container is not empty, `false` if the container is empty.
    explicit operator bool() {
        const std::lock_guard<std::mutex> lock(m_lock);
        return BaseType::operator bool();
    }

private:
    /// Lock to make the class thread-safe
    std::mutex m_lock;
};

}

#endif
