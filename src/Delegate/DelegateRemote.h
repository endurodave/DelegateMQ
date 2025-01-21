#ifndef _DELEGATE_REMOTE_H
#define _DELEGATE_REMOTE_H

// DelegateRemote.h
// @see https://github.com/endurodave/cpp-async-delegate
// David Lafreniere, Aug 2025.

/// @file
/// @brief Delegate "`Remote`" series of classes used to invoke a function remotely 
/// (i.e. different CPU, different process, etc...). 
/// 
/// @details The classes are not thread safe. Invoking a function remotely requires serializing
/// all target function arguments and sending the argument data to the remote destination. All
/// argument data is serialized into a stream. The receiver calls `Invoke()` with the received
/// serialized stream arguments.
/// 
/// An `ISerializer` and `IDispatcher` implementations are required to serialize and dispatch a 
/// remote delegate. 
/// 
/// `RetType operator()(Args... args)` - called by the sender to initiate the remote function call. 
/// Use `SetErrorHandler()' to catch invoke errors. Clone() may throw `std::bad_alloc` unless 
/// `USE_ASSERTS`. All other delegate class functions do not throw exceptions.
///
/// `void Invoke(std::istream& is)` - called by the receiver to invoke the target function. 
/// 
/// Limitations:
/// 
/// * The target function return value is not valid after invoke since the delegate does 
/// not wait for the target function to be called.
/// 
/// * Cannot use a `void*` as a target function argument.
/// 
/// * Cannot use rvalue reference (T&&) as a target function argument.
/// 
/// * Cannot insert `DelegateRemoteAsync` into an ordered container. e.g. `std::list` ok, 
/// `std::set` not ok.
/// 
/// * `std::function` compares the function signature type, not the underlying object instance.
/// See `DelegateFunction<>` class for more info.
/// 
/// Code within `<common_code>` and `</common_code>` is updated using sync_src.py. Manually update 
/// the code within the `DelegateFreeRemote` `common_code` tags, then run the script to 
/// propagate to the remaining delegate classes to simplify code maintenance.
/// 
/// `python src_dup.py DelegateRemote.h`

#include "Delegate.h"
#include "ISerializer.h"
#include "IDispatcher.h"
#include <tuple>
#include <sstream>

namespace DelegateLib {

// Remote identifier shared between sender and receiver remotes
typedef int DelegateRemoteId;
const int INVALID_REMOTE_ID = -1;

enum class DelegateError {
    SUCCESS = 0,
    ERR_STREAM_FAIL = 1,
    ERR_STREAM_EMPTY = 2,
    ERR_NO_SERIALIZER = 3,
    ERR_DISPATCH_ERROR = 4
};

// Get the type of the Nth position within a template parameter pack
template<size_t N, class... Args> using ArgTypeOf =
typename std::tuple_element<N, std::tuple<Args...>>::type;

// Get the value of the Nth position within a template parameter pack
template <size_t N, class... Args>
decltype(auto) ArgValueOf(Args&&... ts) {
    return std::get<N>(std::forward_as_tuple(ts...));
}

template <class Arg>
class RemoteArg
{
public:
    Arg& Get() { return m_arg; }
private:
    Arg m_arg;
};

template <class Arg>
class RemoteArg<Arg*>
{
public:
    Arg* Get() { return &m_arg; }
private:
    Arg m_arg;
};

template <class Arg>
class RemoteArg<Arg**>
{
public:
    RemoteArg() { m_pArg = &m_arg; }
    Arg** Get() { return &m_pArg; }
private:
    Arg m_arg;
    Arg* m_pArg;
};

template <class Arg>
class RemoteArg<Arg&>
{
public:
    Arg& Get() { return m_arg; }
private:
    Arg m_arg;
};

template <class R>
struct DelegateFreeRemote; // Not defined

/// @brief `DelegateFreeRemote<>` class asynchronously invokes a free target function.
/// @tparam RetType The return type of the bound delegate function.
/// @tparam Args The argument types of the bound delegate function.
template <class RetType, class... Args>
class DelegateFreeRemote<RetType(Args...)> : public DelegateFree<RetType(Args...)>, public IRemoteInvoker {
public:
    typedef std::integral_constant<std::size_t, sizeof...(Args)> ArgCnt;
    typedef RetType(*FreeFunc)(Args...);
    using ClassType = DelegateFreeRemote<RetType(Args...)>;
    using BaseType = DelegateFree<RetType(Args...)>;

    /// @brief Constructor to create a class instance. Typically called by sender. 
    /// @param[in] id The remote delegate identifier.
    DelegateFreeRemote(DelegateRemoteId id) : m_id(id) { }

    /// @brief Constructor to create a class instance. Typically called by receiver.
    /// @param[in] func The target free function to store.
    /// @param[in] id The remote delegate identifier.
    DelegateFreeRemote(FreeFunc func, DelegateRemoteId id) :
        BaseType(func), m_id(id) { 
        Bind(func, id); 
    }

    /// @brief Copy constructor that creates a copy of the given instance.
    /// @details This constructor initializes a new object as a copy of the 
    /// provided `rhs` (right-hand side) object. The `rhs` object is used to 
    /// set the state of the new instance.
    /// @param[in] rhs The object to copy from.
    DelegateFreeRemote(const ClassType& rhs) :
        BaseType(rhs), m_id(rhs.m_id) {
        Assign(rhs);
    }

    /// @brief Move constructor that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    DelegateFreeRemote(ClassType&& rhs) noexcept : 
        BaseType(rhs), m_id(rhs.m_id) {
        rhs.Clear();
    }

    DelegateFreeRemote() = default;

    /// @brief Bind a free function to the delegate.
    /// @details This method associates a free function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] func The free function to bind to the delegate. This function must 
    /// match the signature of the delegate.
    /// @param[in] id The remote delegate identifier.
    void Bind(FreeFunc func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(func);
    }

    // <common_code>

    /// @brief Assigns the state of one object to another.
    /// @details Copy the state from the `rhs` (right-hand side) object to the
    /// current object.
    /// @param[in] rhs The object whose state is to be copied.
    void Assign(const ClassType& rhs) {
        m_id = rhs.m_id;
        BaseType::Assign(rhs);
    }

    /// @brief Creates a copy of the current object.
    /// @details Clones the current instance of the class by creating a new object
    /// and copying the state of the current object to it. 
    /// @return A pointer to a new `ClassType` instance.
    /// @post The caller is responsible for deleting the clone object.
    /// @throws std::bad_alloc If dynamic memory allocation fails and USE_ASSERTS not defined.
    virtual ClassType* Clone() const override {
        return new(std::nothrow) ClassType(*this);
    }

    /// @brief Assignment operator that assigns the state of one object to another.
    /// @param[in] rhs The object whose state is to be assigned to the current object.
    /// @return A reference to the current object.
    ClassType& operator=(const ClassType& rhs) {
        if (&rhs != this) {
            BaseType::operator=(rhs);
            Assign(rhs);
        }
        return *this;
    }

    /// @brief Move assignment operator that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    /// @return A reference to the current object.
    ClassType& operator=(ClassType&& rhs) noexcept {
        if (&rhs != this) {
            BaseType::operator=(std::move(rhs));
            m_id = rhs.m_id;    // Use the resource
        }
        return *this;
    }

    /// @brief Clear the target function.
    virtual void operator=(std::nullptr_t) noexcept override {
        return this->Clear();
    }

    /// @brief Compares two delegate objects for equality.
    /// @param[in] rhs The `DelegateBase` object to compare with the current object.
    /// @return `true` if the two delegate objects are equal, `false` otherwise.
    virtual bool Equal(const DelegateBase& rhs) const override {
        auto derivedRhs = dynamic_cast<const ClassType*>(&rhs);
        return derivedRhs &&
            m_id == derivedRhs->m_id &&
            BaseType::Equal(rhs);
    }

    /// Compares two delegate objects for equality.
    /// @return `true` if the objects are equal, `false` otherwise.
    bool operator==(const ClassType& rhs) const noexcept { return Equal(rhs); }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    virtual bool operator==(std::nullptr_t) const noexcept override {
        return this->Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    virtual bool operator!=(std::nullptr_t) const noexcept override {
        return !this->Empty();
    }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    friend bool operator==(std::nullptr_t, const ClassType& rhs) noexcept {
        return rhs.Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    friend bool operator!=(std::nullptr_t, const ClassType& rhs) noexcept {
        return !rhs.Empty();
    }

    /// @brief Invoke the bound delegate function on the remote. Called by the sender.
    /// @details Invoke remote delegate function asynchronously and do not wait for the 
    /// return value. This function is called by the sender. Dispatches the delegate data to 
    /// the destination remote receiver. `Invoke()` must be called by the destination 
    /// remote receiver to invoke the target function. Always safe to call.
    /// 
    /// All argument data is serialized into a binary byte stream. The stream of bytes is 
    /// sent to the receiver. The receivder deserializes the arguments and invokes the remote
    /// target function. 
    /// 
    /// All user-defined argument data must inherit from ISerializer and implement the 
    /// `read()` and `write()` functions to serialize each data member. 
    /// 
    /// Does not throw exceptions. However, the platform-specific `ISerializer` implementation 
    /// might. Register for error callbacks using `SetErrorHandler()`.
    /// 
    /// @param[in] args The function arguments, if any.
    /// @return A default return value. The return value is *not* returned from the 
    /// target function. Do not use the return value.
    /// @post Do not use the return value as its not valid.
    virtual RetType operator()(Args... args) override {
        // Synchronously invoke the target function?
        if (m_sync) {
            if (this->Empty())
                return RetType();

            // Invoke the target function directly
            return BaseType::operator()(std::forward<Args>(args)...);
        }
        else {
            if (m_serializer) {
                xostringstream arg_stream(std::ios::out | std::ios::in | std::ios::binary);

                // Serialize all target function arguments into a stream
                m_serializer->write(arg_stream, std::forward<Args>(args)...);

                if (arg_stream.fail() || arg_stream.bad()) {
                    RaiseError(DelegateError::ERR_STREAM_FAIL);
                }
                else if (arg_stream.eof()) {
                    RaiseError(DelegateError::ERR_STREAM_EMPTY);
                }
                else {
                    // Dispatch delegate invocation to the remote destination
                    if (m_dispatcher) {
                        int error = m_dispatcher->Dispatch(arg_stream);
                        if (error)
                            RaiseError(DelegateError::ERR_DISPATCH_ERROR, error);
                    }
                }
            }

            // Do not wait for remote to invoke function call
            return RetType();

            // Check if any argument is a shared_ptr with wrong usage
            // std::shared_ptr reference arguments are not allowed with asynchronous delegates as the behavior is 
            // undefined. In other words:
            // void MyFunc(std::shared_ptr<T> data)		// Ok!
            // void MyFunc(std::shared_ptr<T>& data)	// Error
            static_assert(!(
                std::disjunction_v<is_shared_ptr_reference<Args>...>),
                "std::shared_ptr reference argument not allowed");
        }
    }

    /// @brief Invoke delegate function asynchronously. Do not wait for return value.
    /// Called by the remote sender. Always safe to call.
    /// @param[in] args The function arguments, if any.
    void AsyncInvoke(Args... args) {
        operator()(std::forward<Args>(args)...);
    }

    /// @brief Invoke the delegate function on the destination receiver. Called by the 
    /// remote destination. The sender serializes all target function arguments. This
    /// function unserializes the argument data and invokes the remote target function.
    /// @details Each source sender call to `operator()` generate a call to `Invoke()` 
    /// on the destination receiver. 
    /// @param[in] is The delegate argument stream created and sent within 
    /// `operator()(Args... args)`.
    /// @return `true` if target function invoked; `false` if error. 
    virtual bool Invoke(std::istream& is) override {
        if (!m_serializer) {
            RaiseError(DelegateError::ERR_NO_SERIALIZER);
            return false;
        }

        // Invoke the delegate function synchronously
        m_sync = true;

        if constexpr (ArgCnt::value == 0) {
            BaseType::operator()();
        }
        else if constexpr (ArgCnt::value == 1) {
            using Arg1 = ArgTypeOf<0, Args...>;

            RemoteArg<Arg1> rp1;
            Arg1 a1 = rp1.Get();

            m_serializer->read(is, a1);
            operator()(a1);
        }
        else if constexpr (ArgCnt::value == 2) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();

            m_serializer->read(is, a1, a2);
            operator()(a1, a2);
        }
        else if constexpr (ArgCnt::value == 3) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();

            m_serializer->read(is, a1, a2, a3);
            operator()(a1, a2, a3);
        }
        else if constexpr (ArgCnt::value == 4) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();

            m_serializer->read(is, a1, a2, a3, a4);
            operator()(a1, a2, a3, a4);
        }
        else if constexpr (ArgCnt::value == 5) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;
            using Arg5 = ArgTypeOf<4, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            RemoteArg<Arg5> rp5;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();
            Arg5 a5 = rp5.Get();

            m_serializer->read(is, a1, a2, a3, a4, a5);
            operator()(a1, a2, a3, a4, a5);
        }
        else {
            static_assert(false, "Too many target function arguments");
        }

        return true;
    }

    ///@brief Get the remote identifier.
    // @return The remote identifier.
    DelegateRemoteId GetRemoteId() noexcept { return m_id; }

    /// @brief Set the dispatcher instance used to send to remote
    /// @param[in] dispatcher A dispatcher instance
    void SetDispatcher(IDispatcher* dispatcher) {
        m_dispatcher = dispatcher;
    }

    /// @brief Set the serializer instance used to serialize/deserialize
    /// function arguments. 
    /// @param[in] serializer A serializer instance
    void SetSerializer(ISerializer<RetType(Args...)>* serializer) {
        m_serializer = serializer;
    }

    /// @brief Set the error handler
    /// @param[in] errorHandler The delegate error handler called when 
    /// an error is detected.
    void SetErrorHandler(Delegate<void(DelegateError, int)>* errorHandler) {
        m_errorHandler = errorHandler;
    }

    /// @brief Get the last error code
    /// @return The last error detected
    /// @post Error is reset to SUCCESS after call
    DelegateError GetError() {
        DelegateError retVal = m_error;
        m_error = DelegateError::SUCCESS;
        return retVal;
    }

private:
    /// @brief Raise an error and callback registered error handler
    /// @param[in] error Error code.
    /// @param[in] auxCode Optional auxiliary code.
    void RaiseError(DelegateError error, int auxCode = 0) {
        if (m_errorHandler)
            (*m_errorHandler)(error, auxCode);
    }

    /// The delegate unique remote identifier
    DelegateRemoteId m_id = INVALID_REMOTE_ID;

    /// A pointer to a error handler callback
    Delegate<void(DelegateError, int)>* m_errorHandler = nullptr;

    /// A pointer to the delegate dispatcher
    IDispatcher* m_dispatcher = nullptr;

    /// A pointer to the function argument serializer
    ISerializer<RetType(Args...)>* m_serializer = nullptr;

    /// Flag to control synchronous vs asynchronous target invoke behavior.
    bool m_sync = false;

    /// The error detected
    DelegateError m_error = DelegateError::SUCCESS;

    // </common_code>
};

template <class C, class R>
struct DelegateMemberRemote; // Not defined

/// @brief `DelegateMemberRemote<>` class asynchronously invokes a class member target function.
/// @tparam TClass The class type that contains the member function.
/// @tparam RetType The return type of the bound delegate function.
/// @tparam Args The argument types of the bound delegate function.
template <class TClass, class RetType, class... Args>
class DelegateMemberRemote<TClass, RetType(Args...)> : public DelegateMember<TClass, RetType(Args...)>, public IRemoteInvoker {
public:
    typedef std::integral_constant<std::size_t, sizeof...(Args)> ArgCnt;
    typedef TClass* ObjectPtr;
    typedef std::shared_ptr<TClass> SharedPtr;
    typedef RetType(TClass::* MemberFunc)(Args...);
    typedef RetType(TClass::* ConstMemberFunc)(Args...) const;
    using ClassType = DelegateMemberRemote<TClass, RetType(Args...)>;
    using BaseType = DelegateMember<TClass, RetType(Args...)>;

    /// @brief Constructor to create a class instance. Typically called by sender. 
    /// @param[in] id The remote delegate identifier.
    DelegateMemberRemote(DelegateRemoteId id) : m_id(id) { }

    /// @brief Constructor to create a class instance. Typically called by receiver. 
    /// @param[in] object The target object pointer to store.
    /// @param[in] func The target member function to store.
    /// @param[in] id The delegate remote identifier.
    DelegateMemberRemote(SharedPtr object, MemberFunc func, DelegateRemoteId id) : BaseType(object, func), m_id(id) {
        Bind(object, func, id);
    }

    /// @brief Constructor to create a class instance. Typically called by receiver. 
    /// @param[in] object The target object pointer to store.
    /// @param[in] func The target const member function to store.
    /// @param[in] id The delegate remote identifier.
    DelegateMemberRemote(SharedPtr object, ConstMemberFunc func, DelegateRemoteId id) : BaseType(object, func), m_id(id) {
        Bind(object, func, id);
    }

    /// @brief Constructor to create a class instance. Typically called by receiver. 
    /// @param[in] object The target object pointer to store.
    /// @param[in] func The target member function to store.
    /// @param[in] id The delegate remote identifier.
    DelegateMemberRemote(ObjectPtr object, MemberFunc func, DelegateRemoteId id) : BaseType(object, func), m_id(id) {
        Bind(object, func, id);
    }

    /// @brief Constructor to create a class instance. Typically called by receiver. 
    /// @param[in] object The target object pointer to store.
    /// @param[in] func The target const member function to store.
    /// @param[in] id The delegate remote identifier.
    DelegateMemberRemote(ObjectPtr object, ConstMemberFunc func, DelegateRemoteId id) : BaseType(object, func), m_id(id) {
        Bind(object, func, id);
    }

    /// @brief Copy constructor that creates a copy of the given instance.
    /// @details This constructor initializes a new object as a copy of the 
    /// provided `rhs` (right-hand side) object. The `rhs` object is used to 
    /// set the state of the new instance.
    /// @param[in] rhs The object to copy from.
    DelegateMemberRemote(const ClassType& rhs) :
        BaseType(rhs), m_id(rhs.m_id) {
        Assign(rhs);
    }

    /// @brief Move constructor that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    DelegateMemberRemote(ClassType&& rhs) noexcept :
        BaseType(rhs), m_id(rhs.m_id) {
        rhs.Clear();
    }

    DelegateMemberRemote() = default;

    /// @brief Bind a const member function to the delegate.
    /// @details This method associates a member function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] object The target object instance.
    /// @param[in] func The function to bind to the delegate. This function must match 
    /// the signature of the delegate.
    /// @param[in] id The delegate remote identifier.
    void Bind(SharedPtr object, MemberFunc func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(object, func);
    }

    /// @brief Bind a member function to the delegate.
    /// @details This method associates a member function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] object The target object instance.
    /// @param[in] func The member function to bind to the delegate. This function must 
    /// match the signature of the delegate.
    /// @param[in] id The delegate remote identifier.
    void Bind(SharedPtr object, ConstMemberFunc func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(object, func);
    }

    /// @brief Bind a const member function to the delegate.
    /// @details This method associates a member function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] object The target object instance.
    /// @param[in] func The function to bind to the delegate. This function must match 
    /// the signature of the delegate.
    /// @param[in] id The delegate remote identifier.
    void Bind(ObjectPtr object, MemberFunc func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(object, func);
    }

    /// @brief Bind a member function to the delegate.
    /// @details This method associates a member function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] object The target object instance.
    /// @param[in] func The member function to bind to the delegate. This function must 
    /// match the signature of the delegate.
    /// @param[in] id The delegate remote identifier.
    void Bind(ObjectPtr object, ConstMemberFunc func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(object, func);
    }

    // <common_code>

    /// @brief Assigns the state of one object to another.
    /// @details Copy the state from the `rhs` (right-hand side) object to the
    /// current object.
    /// @param[in] rhs The object whose state is to be copied.
    void Assign(const ClassType& rhs) {
        m_id = rhs.m_id;
        BaseType::Assign(rhs);
    }

    /// @brief Creates a copy of the current object.
    /// @details Clones the current instance of the class by creating a new object
    /// and copying the state of the current object to it. 
    /// @return A pointer to a new `ClassType` instance.
    /// @post The caller is responsible for deleting the clone object.
    /// @throws std::bad_alloc If dynamic memory allocation fails and USE_ASSERTS not defined.
    virtual ClassType* Clone() const override {
        return new(std::nothrow) ClassType(*this);
    }

    /// @brief Assignment operator that assigns the state of one object to another.
    /// @param[in] rhs The object whose state is to be assigned to the current object.
    /// @return A reference to the current object.
    ClassType& operator=(const ClassType& rhs) {
        if (&rhs != this) {
            BaseType::operator=(rhs);
            Assign(rhs);
        }
        return *this;
    }

    /// @brief Move assignment operator that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    /// @return A reference to the current object.
    ClassType& operator=(ClassType&& rhs) noexcept {
        if (&rhs != this) {
            BaseType::operator=(std::move(rhs));
            m_id = rhs.m_id;    // Use the resource
        }
        return *this;
    }

    /// @brief Clear the target function.
    virtual void operator=(std::nullptr_t) noexcept override {
        return this->Clear();
    }

    /// @brief Compares two delegate objects for equality.
    /// @param[in] rhs The `DelegateBase` object to compare with the current object.
    /// @return `true` if the two delegate objects are equal, `false` otherwise.
    virtual bool Equal(const DelegateBase& rhs) const override {
        auto derivedRhs = dynamic_cast<const ClassType*>(&rhs);
        return derivedRhs &&
            m_id == derivedRhs->m_id &&
            BaseType::Equal(rhs);
    }

    /// Compares two delegate objects for equality.
    /// @return `true` if the objects are equal, `false` otherwise.
    bool operator==(const ClassType& rhs) const noexcept { return Equal(rhs); }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    virtual bool operator==(std::nullptr_t) const noexcept override {
        return this->Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    virtual bool operator!=(std::nullptr_t) const noexcept override {
        return !this->Empty();
    }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    friend bool operator==(std::nullptr_t, const ClassType& rhs) noexcept {
        return rhs.Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    friend bool operator!=(std::nullptr_t, const ClassType& rhs) noexcept {
        return !rhs.Empty();
    }

    /// @brief Invoke the bound delegate function on the remote. Called by the sender.
    /// @details Invoke remote delegate function asynchronously and do not wait for the 
    /// return value. This function is called by the sender. Dispatches the delegate data to 
    /// the destination remote receiver. `Invoke()` must be called by the destination 
    /// remote receiver to invoke the target function. Always safe to call.
    /// 
    /// All argument data is serialized into a binary byte stream. The stream of bytes is 
    /// sent to the receiver. The receivder deserializes the arguments and invokes the remote
    /// target function. 
    /// 
    /// All user-defined argument data must inherit from ISerializer and implement the 
    /// `read()` and `write()` functions to serialize each data member. 
    /// 
    /// Does not throw exceptions. However, the platform-specific `ISerializer` implementation 
    /// might. Register for error callbacks using `SetErrorHandler()`.
    /// 
    /// @param[in] args The function arguments, if any.
    /// @return A default return value. The return value is *not* returned from the 
    /// target function. Do not use the return value.
    /// @post Do not use the return value as its not valid.
    virtual RetType operator()(Args... args) override {
        // Synchronously invoke the target function?
        if (m_sync) {
            if (this->Empty())
                return RetType();

            // Invoke the target function directly
            return BaseType::operator()(std::forward<Args>(args)...);
        }
        else {
            if (m_serializer) {
                xostringstream arg_stream(std::ios::out | std::ios::in | std::ios::binary);

                // Serialize all target function arguments into a stream
                m_serializer->write(arg_stream, std::forward<Args>(args)...);

                if (arg_stream.fail() || arg_stream.bad()) {
                    RaiseError(DelegateError::ERR_STREAM_FAIL);
                }
                else if (arg_stream.eof()) {
                    RaiseError(DelegateError::ERR_STREAM_EMPTY);
                }
                else {
                    // Dispatch delegate invocation to the remote destination
                    if (m_dispatcher) {
                        int error = m_dispatcher->Dispatch(arg_stream);
                        if (error)
                            RaiseError(DelegateError::ERR_DISPATCH_ERROR, error);
                    }
                }
            }

            // Do not wait for remote to invoke function call
            return RetType();

            // Check if any argument is a shared_ptr with wrong usage
            // std::shared_ptr reference arguments are not allowed with asynchronous delegates as the behavior is 
            // undefined. In other words:
            // void MyFunc(std::shared_ptr<T> data)		// Ok!
            // void MyFunc(std::shared_ptr<T>& data)	// Error
            static_assert(!(
                std::disjunction_v<is_shared_ptr_reference<Args>...>),
                "std::shared_ptr reference argument not allowed");
        }
    }

    /// @brief Invoke delegate function asynchronously. Do not wait for return value.
    /// Called by the remote sender. Always safe to call.
    /// @param[in] args The function arguments, if any.
    void AsyncInvoke(Args... args) {
        operator()(std::forward<Args>(args)...);
    }

    /// @brief Invoke the delegate function on the destination receiver. Called by the 
    /// remote destination. The sender serializes all target function arguments. This
    /// function unserializes the argument data and invokes the remote target function.
    /// @details Each source sender call to `operator()` generate a call to `Invoke()` 
    /// on the destination receiver. 
    /// @param[in] is The delegate argument stream created and sent within 
    /// `operator()(Args... args)`.
    /// @return `true` if target function invoked; `false` if error. 
    virtual bool Invoke(std::istream& is) override {
        if (!m_serializer) {
            RaiseError(DelegateError::ERR_NO_SERIALIZER);
            return false;
        }

        // Invoke the delegate function synchronously
        m_sync = true;

        if constexpr (ArgCnt::value == 0) {
            BaseType::operator()();
        }
        else if constexpr (ArgCnt::value == 1) {
            using Arg1 = ArgTypeOf<0, Args...>;

            RemoteArg<Arg1> rp1;
            Arg1 a1 = rp1.Get();

            m_serializer->read(is, a1);
            operator()(a1);
        }
        else if constexpr (ArgCnt::value == 2) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();

            m_serializer->read(is, a1, a2);
            operator()(a1, a2);
        }
        else if constexpr (ArgCnt::value == 3) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();

            m_serializer->read(is, a1, a2, a3);
            operator()(a1, a2, a3);
        }
        else if constexpr (ArgCnt::value == 4) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();

            m_serializer->read(is, a1, a2, a3, a4);
            operator()(a1, a2, a3, a4);
        }
        else if constexpr (ArgCnt::value == 5) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;
            using Arg5 = ArgTypeOf<4, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            RemoteArg<Arg5> rp5;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();
            Arg5 a5 = rp5.Get();

            m_serializer->read(is, a1, a2, a3, a4, a5);
            operator()(a1, a2, a3, a4, a5);
        }
        else {
            static_assert(false, "Too many target function arguments");
        }

        return true;
    }

    ///@brief Get the remote identifier.
    // @return The remote identifier.
    DelegateRemoteId GetRemoteId() noexcept { return m_id; }

    /// @brief Set the dispatcher instance used to send to remote
    /// @param[in] dispatcher A dispatcher instance
    void SetDispatcher(IDispatcher* dispatcher) {
        m_dispatcher = dispatcher;
    }

    /// @brief Set the serializer instance used to serialize/deserialize
    /// function arguments. 
    /// @param[in] serializer A serializer instance
    void SetSerializer(ISerializer<RetType(Args...)>* serializer) {
        m_serializer = serializer;
    }

    /// @brief Set the error handler
    /// @param[in] errorHandler The delegate error handler called when 
    /// an error is detected.
    void SetErrorHandler(Delegate<void(DelegateError, int)>* errorHandler) {
        m_errorHandler = errorHandler;
    }

    /// @brief Get the last error code
    /// @return The last error detected
    /// @post Error is reset to SUCCESS after call
    DelegateError GetError() {
        DelegateError retVal = m_error;
        m_error = DelegateError::SUCCESS;
        return retVal;
    }

private:
    /// @brief Raise an error and callback registered error handler
    /// @param[in] error Error code.
    /// @param[in] auxCode Optional auxiliary code.
    void RaiseError(DelegateError error, int auxCode = 0) {
        if (m_errorHandler)
            (*m_errorHandler)(error, auxCode);
    }

    /// The delegate unique remote identifier
    DelegateRemoteId m_id = INVALID_REMOTE_ID;

    /// A pointer to a error handler callback
    Delegate<void(DelegateError, int)>* m_errorHandler = nullptr;

    /// A pointer to the delegate dispatcher
    IDispatcher* m_dispatcher = nullptr;

    /// A pointer to the function argument serializer
    ISerializer<RetType(Args...)>* m_serializer = nullptr;

    /// Flag to control synchronous vs asynchronous target invoke behavior.
    bool m_sync = false;

    /// The error detected
    DelegateError m_error = DelegateError::SUCCESS;

    // </common_code>
};

template <class R>
struct DelegateFunctionRemote; // Not defined

/// @brief `DelegateFunctionRemote<>` class asynchronously invokes a `std::function` target function.
/// @details Caution when binding to a `std::function` using this class. `std::function` cannot be 
/// compared for equality directly in a meaningful way using `operator==`. Therefore, the delegate
/// library used 
/// 
/// See `DelegateFunction<>` base class for important usage limitations.
/// 
/// @tparam RetType The return type of the bound delegate function.
/// @tparam Args The argument types of the bound delegate function.
template <class RetType, class... Args>
class DelegateFunctionRemote<RetType(Args...)> : public DelegateFunction<RetType(Args...)>, public IRemoteInvoker {
public:
    typedef std::integral_constant<std::size_t, sizeof...(Args)> ArgCnt;
    using FunctionType = std::function<RetType(Args...)>;
    using ClassType = DelegateFunctionRemote<RetType(Args...)>;
    using BaseType = DelegateFunction<RetType(Args...)>;

    /// @brief Constructor to create a class instance. Typically called by sender. 
    /// @param[in] id The remote delegate identifier.
    DelegateFunctionRemote(DelegateRemoteId id) : m_id(id) { }

    /// @brief Constructor to create a class instance. Typically called by receiver.
    /// @param[in] func The target `std::function` to store.
    /// @param[in] id The unique remote delegate identifier.
    DelegateFunctionRemote(FunctionType func, DelegateRemoteId id) :
        BaseType(func), m_id(id) {
        Bind(func, id);
    }

    /// @brief Copy constructor that creates a copy of the given instance.
    /// @details This constructor initializes a new object as a copy of the 
    /// provided `rhs` (right-hand side) object. The `rhs` object is used to 
    /// set the state of the new instance.
    /// @param[in] rhs The object to copy from.
    DelegateFunctionRemote(const ClassType& rhs) :
        BaseType(rhs), m_id(rhs.m_id) {
        Assign(rhs);
    }

    /// @brief Move constructor that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    DelegateFunctionRemote(ClassType&& rhs) noexcept :
        BaseType(rhs), m_id(rhs.m_id) {
        rhs.Clear();
    }

    DelegateFunctionRemote() = default;

    /// @brief Bind a `std::function` to the delegate.
    /// @details This method associates a member function (`func`) with the delegate. 
    /// Once the function is bound, the delegate can be used to invoke the function.
    /// @param[in] func The `std::function` to bind to the delegate. This function must match 
    /// the signature of the delegate.
    /// @param[in] id The delegate remote identifier.
    void Bind(FunctionType func, DelegateRemoteId id) {
        m_id = id;
        BaseType::Bind(func);
    }

    // <common_code>

    /// @brief Assigns the state of one object to another.
    /// @details Copy the state from the `rhs` (right-hand side) object to the
    /// current object.
    /// @param[in] rhs The object whose state is to be copied.
    void Assign(const ClassType& rhs) {
        m_id = rhs.m_id;
        BaseType::Assign(rhs);
    }

    /// @brief Creates a copy of the current object.
    /// @details Clones the current instance of the class by creating a new object
    /// and copying the state of the current object to it. 
    /// @return A pointer to a new `ClassType` instance.
    /// @post The caller is responsible for deleting the clone object.
    /// @throws std::bad_alloc If dynamic memory allocation fails and USE_ASSERTS not defined.
    virtual ClassType* Clone() const override {
        return new(std::nothrow) ClassType(*this);
    }

    /// @brief Assignment operator that assigns the state of one object to another.
    /// @param[in] rhs The object whose state is to be assigned to the current object.
    /// @return A reference to the current object.
    ClassType& operator=(const ClassType& rhs) {
        if (&rhs != this) {
            BaseType::operator=(rhs);
            Assign(rhs);
        }
        return *this;
    }

    /// @brief Move assignment operator that transfers ownership of resources.
    /// @param[in] rhs The object to move from.
    /// @return A reference to the current object.
    ClassType& operator=(ClassType&& rhs) noexcept {
        if (&rhs != this) {
            BaseType::operator=(std::move(rhs));
            m_id = rhs.m_id;    // Use the resource
        }
        return *this;
    }

    /// @brief Clear the target function.
    virtual void operator=(std::nullptr_t) noexcept override {
        return this->Clear();
    }

    /// @brief Compares two delegate objects for equality.
    /// @param[in] rhs The `DelegateBase` object to compare with the current object.
    /// @return `true` if the two delegate objects are equal, `false` otherwise.
    virtual bool Equal(const DelegateBase& rhs) const override {
        auto derivedRhs = dynamic_cast<const ClassType*>(&rhs);
        return derivedRhs &&
            m_id == derivedRhs->m_id &&
            BaseType::Equal(rhs);
    }

    /// Compares two delegate objects for equality.
    /// @return `true` if the objects are equal, `false` otherwise.
    bool operator==(const ClassType& rhs) const noexcept { return Equal(rhs); }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    virtual bool operator==(std::nullptr_t) const noexcept override {
        return this->Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    virtual bool operator!=(std::nullptr_t) const noexcept override {
        return !this->Empty();
    }

    /// Overload operator== to compare the delegate to nullptr
    /// @return `true` if delegate is null.
    friend bool operator==(std::nullptr_t, const ClassType& rhs) noexcept {
        return rhs.Empty();
    }

    /// Overload operator!= to compare the delegate to nullptr
    /// @return `true` if delegate is not null.
    friend bool operator!=(std::nullptr_t, const ClassType& rhs) noexcept {
        return !rhs.Empty();
    }

    /// @brief Invoke the bound delegate function on the remote. Called by the sender.
    /// @details Invoke remote delegate function asynchronously and do not wait for the 
    /// return value. This function is called by the sender. Dispatches the delegate data to 
    /// the destination remote receiver. `Invoke()` must be called by the destination 
    /// remote receiver to invoke the target function. Always safe to call.
    /// 
    /// All argument data is serialized into a binary byte stream. The stream of bytes is 
    /// sent to the receiver. The receivder deserializes the arguments and invokes the remote
    /// target function. 
    /// 
    /// All user-defined argument data must inherit from ISerializer and implement the 
    /// `read()` and `write()` functions to serialize each data member. 
    /// 
    /// Does not throw exceptions. However, the platform-specific `ISerializer` implementation 
    /// might. Register for error callbacks using `SetErrorHandler()`.
    /// 
    /// @param[in] args The function arguments, if any.
    /// @return A default return value. The return value is *not* returned from the 
    /// target function. Do not use the return value.
    /// @post Do not use the return value as its not valid.
    virtual RetType operator()(Args... args) override {
        // Synchronously invoke the target function?
        if (m_sync) {
            if (this->Empty())
                return RetType();

            // Invoke the target function directly
            return BaseType::operator()(std::forward<Args>(args)...);
        }
        else {
            if (m_serializer) {
                xostringstream arg_stream(std::ios::out | std::ios::in | std::ios::binary);

                // Serialize all target function arguments into a stream
                m_serializer->write(arg_stream, std::forward<Args>(args)...);

                if (arg_stream.fail() || arg_stream.bad()) {
                    RaiseError(DelegateError::ERR_STREAM_FAIL);
                }
                else if (arg_stream.eof()) {
                    RaiseError(DelegateError::ERR_STREAM_EMPTY);
                }
                else {
                    // Dispatch delegate invocation to the remote destination
                    if (m_dispatcher) {
                        int error = m_dispatcher->Dispatch(arg_stream);
                        if (error)
                            RaiseError(DelegateError::ERR_DISPATCH_ERROR, error);
                    }
                }
            }

            // Do not wait for remote to invoke function call
            return RetType();

            // Check if any argument is a shared_ptr with wrong usage
            // std::shared_ptr reference arguments are not allowed with asynchronous delegates as the behavior is 
            // undefined. In other words:
            // void MyFunc(std::shared_ptr<T> data)		// Ok!
            // void MyFunc(std::shared_ptr<T>& data)	// Error
            static_assert(!(
                std::disjunction_v<is_shared_ptr_reference<Args>...>),
                "std::shared_ptr reference argument not allowed");
        }
    }

    /// @brief Invoke delegate function asynchronously. Do not wait for return value.
    /// Called by the remote sender. Always safe to call.
    /// @param[in] args The function arguments, if any.
    void AsyncInvoke(Args... args) {
        operator()(std::forward<Args>(args)...);
    }

    /// @brief Invoke the delegate function on the destination receiver. Called by the 
    /// remote destination. The sender serializes all target function arguments. This
    /// function unserializes the argument data and invokes the remote target function.
    /// @details Each source sender call to `operator()` generate a call to `Invoke()` 
    /// on the destination receiver. 
    /// @param[in] is The delegate argument stream created and sent within 
    /// `operator()(Args... args)`.
    /// @return `true` if target function invoked; `false` if error. 
    virtual bool Invoke(std::istream& is) override {
        if (!m_serializer) {
            RaiseError(DelegateError::ERR_NO_SERIALIZER);
            return false;
        }

        // Invoke the delegate function synchronously
        m_sync = true;

        if constexpr (ArgCnt::value == 0) {
            BaseType::operator()();
        }
        else if constexpr (ArgCnt::value == 1) {
            using Arg1 = ArgTypeOf<0, Args...>;

            RemoteArg<Arg1> rp1;
            Arg1 a1 = rp1.Get();

            m_serializer->read(is, a1);
            operator()(a1);
        }
        else if constexpr (ArgCnt::value == 2) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();

            m_serializer->read(is, a1, a2);
            operator()(a1, a2);
        }
        else if constexpr (ArgCnt::value == 3) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();

            m_serializer->read(is, a1, a2, a3);
            operator()(a1, a2, a3);
        }
        else if constexpr (ArgCnt::value == 4) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();

            m_serializer->read(is, a1, a2, a3, a4);
            operator()(a1, a2, a3, a4);
        }
        else if constexpr (ArgCnt::value == 5) {
            using Arg1 = ArgTypeOf<0, Args...>;
            using Arg2 = ArgTypeOf<1, Args...>;
            using Arg3 = ArgTypeOf<2, Args...>;
            using Arg4 = ArgTypeOf<3, Args...>;
            using Arg5 = ArgTypeOf<4, Args...>;

            RemoteArg<Arg1> rp1;
            RemoteArg<Arg2> rp2;
            RemoteArg<Arg3> rp3;
            RemoteArg<Arg4> rp4;
            RemoteArg<Arg5> rp5;
            Arg1 a1 = rp1.Get();
            Arg2 a2 = rp2.Get();
            Arg3 a3 = rp3.Get();
            Arg4 a4 = rp4.Get();
            Arg5 a5 = rp5.Get();

            m_serializer->read(is, a1, a2, a3, a4, a5);
            operator()(a1, a2, a3, a4, a5);
        }
        else {
            static_assert(false, "Too many target function arguments");
        }

        return true;
    }

    ///@brief Get the remote identifier.
    // @return The remote identifier.
    DelegateRemoteId GetRemoteId() noexcept { return m_id; }

    /// @brief Set the dispatcher instance used to send to remote
    /// @param[in] dispatcher A dispatcher instance
    void SetDispatcher(IDispatcher* dispatcher) {
        m_dispatcher = dispatcher;
    }

    /// @brief Set the serializer instance used to serialize/deserialize
    /// function arguments. 
    /// @param[in] serializer A serializer instance
    void SetSerializer(ISerializer<RetType(Args...)>* serializer) {
        m_serializer = serializer;
    }

    /// @brief Set the error handler
    /// @param[in] errorHandler The delegate error handler called when 
    /// an error is detected.
    void SetErrorHandler(Delegate<void(DelegateError, int)>* errorHandler) {
        m_errorHandler = errorHandler;
    }

    /// @brief Get the last error code
    /// @return The last error detected
    /// @post Error is reset to SUCCESS after call
    DelegateError GetError() {
        DelegateError retVal = m_error;
        m_error = DelegateError::SUCCESS;
        return retVal;
    }

private:
    /// @brief Raise an error and callback registered error handler
    /// @param[in] error Error code.
    /// @param[in] auxCode Optional auxiliary code.
    void RaiseError(DelegateError error, int auxCode = 0) {
        if (m_errorHandler)
            (*m_errorHandler)(error, auxCode);
    }

    /// The delegate unique remote identifier
    DelegateRemoteId m_id = INVALID_REMOTE_ID;

    /// A pointer to a error handler callback
    Delegate<void(DelegateError, int)>* m_errorHandler = nullptr;

    /// A pointer to the delegate dispatcher
    IDispatcher* m_dispatcher = nullptr;

    /// A pointer to the function argument serializer
    ISerializer<RetType(Args...)>* m_serializer = nullptr;

    /// Flag to control synchronous vs asynchronous target invoke behavior.
    bool m_sync = false;

    /// The error detected
    DelegateError m_error = DelegateError::SUCCESS;

    // </common_code>
};

/// @brief Creates an asynchronous delegate that binds to a free function.
/// @tparam RetType The return type of the free function.
/// @tparam Args The types of the function arguments.
/// @param[in] func A pointer to the free function to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateFreeRemote` object bound to the specified free function and id.
template <class RetType, class... Args>
auto MakeDelegate(RetType(*func)(Args... args), DelegateRemoteId id) {
    return DelegateFreeRemote<RetType(Args...)>(func, id);
}

/// @brief Creates an asynchronous delegate that binds to a non-const member function.
/// @tparam TClass The class type that contains the member function.
/// @tparam RetType The return type of the member function.
/// @tparam Args The types of the function arguments.
/// @param[in] object A pointer to the instance of `TClass` that will be used for the delegate.
/// @param[in] func A pointer to the non-const member function of `TClass` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateMemberRemote` object bound to the specified non-const member function and id.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(TClass* object, RetType(TClass::* func)(Args... args), DelegateRemoteId id) {
    return DelegateMemberRemote<TClass, RetType(Args...)>(object, func, id);
}

/// @brief Creates an asynchronous delegate that binds to a const member function.
/// @tparam TClass The class type that contains the const member function.
/// @tparam RetType The return type of the member function.
/// @tparam Args The types of the function arguments.
/// @param[in] object A pointer to the instance of `TClass` that will be used for the delegate.
/// @param[in] func A pointer to the const member function of `TClass` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateMemberRemote` object bound to the specified const member function and id.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(TClass* object, RetType(TClass::* func)(Args... args) const, DelegateRemoteId id) {
    return DelegateMemberRemote<TClass, RetType(Args...)>(object, func, id);
}

/// @brief Creates a delegate that binds to a const member function.
/// @tparam TClass The const class type that contains the const member function.
/// @tparam RetType The return type of the member function.
/// @tparam Args The types of the function arguments.
/// @param[in] object A pointer to the instance of `TClass` that will be used for the delegate.
/// @param[in] func A pointer to the non-const member function of `TClass` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateMemberRemote` object bound to the specified non-const member function.
template <class TClass, class RetType, class... Args>
auto MakeDelegate(const TClass* object, RetType(TClass::* func)(Args... args) const, DelegateRemoteId id) {
    return DelegateMemberRemote<const TClass, RetType(Args...)>(object, func, id);
}

/// @brief Creates an asynchronous delegate that binds to a non-const member function using a shared pointer.
/// @tparam TClass The class type that contains the member function.
/// @tparam RetType The return type of the member function.
/// @tparam Args The types of the function arguments.
/// @param[in] object A shared pointer to the instance of `TClass` that will be used for the delegate.
/// @param[in] func A pointer to the non-const member function of `TClass` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateMemberRemote` shared pointer bound to the specified non-const member function and id.
template <class TClass, class RetVal, class... Args>
auto MakeDelegate(std::shared_ptr<TClass> object, RetVal(TClass::* func)(Args... args), DelegateRemoteId id) {
    return DelegateMemberRemote<TClass, RetVal(Args...)>(object, func, id);
}

/// @brief Creates an asynchronous delegate that binds to a const member function using a shared pointer.
/// @tparam TClass The class type that contains the member function.
/// @tparam RetVal The return type of the member function.
/// @tparam Args The types of the function arguments.
/// @param[in] object A shared pointer to the instance of `TClass` that will be used for the delegate.
/// @param[in] func A pointer to the const member function of `TClass` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateMemberRemote` shared pointer bound to the specified const member function and id.
template <class TClass, class RetVal, class... Args>
auto MakeDelegate(std::shared_ptr<TClass> object, RetVal(TClass::* func)(Args... args) const, DelegateRemoteId id) {
    return DelegateMemberRemote<TClass, RetVal(Args...)>(object, func, id);
}

/// @brief Creates an asynchronous delegate that binds to a `std::function`.
/// @tparam RetType The return type of the `std::function`.
/// @tparam Args The types of the function arguments.
/// @param[in] func The `std::function` to bind to the delegate.
/// @param[in] id The delegate remote identifier.
/// @return A `DelegateFunctionRemote` object bound to the specified `std::function` and id.
template <class RetType, class... Args>
auto MakeDelegate(std::function<RetType(Args...)> func, DelegateRemoteId id) {
    return DelegateFunctionRemote<RetType(Args...)>(func, id);
}

}

#endif
