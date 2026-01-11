#ifndef _MULTICAST_DELEGATE_SAFE_H
#define _MULTICAST_DELEGATE_SAFE_H

/// @file
/// @brief Delegate container for storing and iterating over a collection of 
/// delegate instances. Class is thread-safe.

#include "MulticastDelegate.h"

namespace dmq {

template <class R>
struct MulticastDelegateSafe; // Not defined

/// @brief Thread-safe multicast delegate container class. 
template<class RetType, class... Args>
class MulticastDelegateSafe<RetType(Args...)> : public MulticastDelegate<RetType(Args...)>
{
public:
    using DelegateType = Delegate<RetType(Args...)>;
    using BaseType = MulticastDelegate<RetType(Args...)>;

    MulticastDelegateSafe() = default;
    ~MulticastDelegateSafe() = default; 

    MulticastDelegateSafe(const MulticastDelegateSafe& rhs) : BaseType() {
        std::scoped_lock lock(m_lock, rhs.m_lock);
        BaseType::operator=(rhs);
    }

    MulticastDelegateSafe(MulticastDelegateSafe&& rhs) : BaseType() {
        std::scoped_lock lock(m_lock, rhs.m_lock);
        BaseType::operator=(std::move(rhs));
    }

    /// Invoke the bound target function for all stored delegate instances.
    /// A void return value is used since multiple targets invoked.
    /// @param[in] args The arguments used when invoking the target functions
    void operator()(Args... args) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::operator ()(args...);
    }

    /// Invoke all bound target functions. A void return value is used 
    /// since multiple targets invoked.
    /// @param[in] args The arguments used when invoking the target functions
    void Broadcast(Args... args) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::Broadcast(args...);
    }

    /// Insert a delegate into the container.
    /// @param[in] delegate A delegate target to insert
    void operator+=(const Delegate<RetType(Args...)>& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::operator +=(delegate);
    }

    /// Insert a delegate into the container.
    /// @param[in] delegate A delegate target to insert
    void operator+=(Delegate<RetType(Args...)>&& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::operator +=(delegate);
    }

    /// Remove a delegate from the container.
    /// @param[in] delegate A delegate target to remove
    void operator-=(const Delegate<RetType(Args...)>& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::operator -=(delegate);
    }

    /// Remove a delegate from the container.
    /// @param[in] delegate A delegate target to remove
    void operator-=(Delegate<RetType(Args...)>&& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::operator -=(delegate);
    }

    /// @brief Assignment operator that assigns the state of one object to another.
    /// @param[in] rhs The object whose state is to be assigned to the current object.
    /// @return A reference to the current object.
    MulticastDelegateSafe& operator=(const MulticastDelegateSafe& rhs) {
        if (this != &rhs) {
            // Lock both instances safely to prevent modification of source during copy
            std::scoped_lock lock(m_lock, rhs.m_lock);
            BaseType::operator=(rhs);
        }
        return *this;
    }

    /// @brief Move assignment operator that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    /// @return A reference to the current object.
    MulticastDelegateSafe& operator=(MulticastDelegateSafe&& rhs) noexcept {
        if (this != &rhs) {
            std::scoped_lock lock(m_lock, rhs.m_lock);
            BaseType::operator=(std::move(rhs));
        }
        return *this;
    }

    /// @brief Clear the all target functions.
    virtual void operator=(std::nullptr_t) noexcept { 
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::Clear(); 
    }

    /// Insert a delegate into the container.
    /// @param[in] delegate A delegate target to insert
    void PushBack(const DelegateType& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::PushBack(delegate);
    }

    /// Remove a delegate into the container.
    /// @param[in] delegate The delegate target to remove.
    void Remove(const DelegateType& delegate) {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        BaseType::Remove(delegate);
    }

    /// Any registered delegates?
    /// @return `true` if delegate container is empty.
    bool Empty() const {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        return BaseType::Empty();
    }

    /// Removal all registered delegates.
    void Clear() {
       const std::lock_guard<RecursiveMutex> lock(m_lock);
       BaseType::Clear();
    }

    /// Get the number of delegates stored.
    /// @return The number of delegates stored.
    std::size_t Size() const { 
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        return BaseType::Size(); 
    }

    /// @brief Implicit conversion operator to `bool`.
    /// @return `true` if the container is not empty, `false` if the container is empty.
    explicit operator bool() const {
        const std::lock_guard<RecursiveMutex> lock(m_lock);
        return BaseType::operator bool();
    }

private:
    /// Lock to make the class thread-safe
    mutable RecursiveMutex m_lock;
};

}

#endif
