/// @file
/// @brief Implement an asynchronous API using template helper function.
/// Includes an async timeout example showing finite-wait delegate invocation.

#include "AsyncAPI.h"
#include "DelegateMQ.h"
#include <iostream>
#include <future>
#include <chrono>

using namespace dmq;
using namespace dmq::os;
using namespace dmq::util;
using namespace std;

namespace Example
{
    // -------------------------------------------------------------------------
    // Encapsulate helpers to avoid static globals
    // -------------------------------------------------------------------------
    class AsyncApiHelpers {
    public:
        static bool _mode;

        static size_t send_data_private(const std::string& data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cout << data << endl;
            return data.size();
        }

        static void set_mode_private(bool mode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            _mode = mode;
        }

        static bool get_mode_private() { return _mode; }
    };
    bool AsyncApiHelpers::_mode = false;

    // -------------------------------------------------------------------------
    // Communication Class
    // -------------------------------------------------------------------------
    /// @brief A class demonstrating asynchronous member function calls.
    ///
    /// This class inherits from `std::enable_shared_from_this` to safely
    /// pass shared pointers of itself to asynchronous delegates, ensuring
    /// the object remains alive during the async call execution.
    class Communication : public std::enable_shared_from_this<Communication>
    {
    public:
        /// @brief Constructor.
        /// @param t Reference to the worker thread where tasks will execute.
        Communication(Thread& t) : m_thread(t) {}

        /// @brief Asynchronously sends data using `AsyncInvoke` on member function.
        ///
        /// This method marshals a call to `SendDataPrivate` onto `m_thread`.
        /// It waits for the result (synchronous wait on async result) due to WAIT_INFINITE.
        /// @param data The data string.
        /// @return Size of data sent.
        size_t SendData(const std::string& data) {
            return AsyncInvoke(shared_from_this(), &Communication::SendDataPrivate, m_thread, WAIT_INFINITE, data);
        }

        /// @brief Asynchronously sends data using `MakeDelegate` manually.
        ///
        /// This version checks if the caller is already on the target thread.
        /// If not, it creates a delegate to `SendDataV2` on `m_thread` and invokes it.
        /// If already on the thread, it executes the logic directly.
        /// @param data The data string.
        /// @return Size of data sent.
        size_t SendDataV2(const std::string& data) {
            if (!m_thread.IsCurrentThread())
                return MakeDelegate(shared_from_this(), &Communication::SendDataV2, m_thread, WAIT_INFINITE)(data);
            cout << data << endl;
            return data.size();
        }

        /// @brief Asynchronously sends data using a lambda.
        ///
        /// Captures `self` to ensure the Communication object stays alive.
        /// @param data The data string.
        /// @return Size of data sent.
        size_t SendDataV3(const std::string& data) {
            auto self = shared_from_this();
            auto sendDataLambda = [self](const std::string& data) -> size_t {
                cout << data << endl;
                return data.size();
                };
            return AsyncInvoke(sendDataLambda, m_thread, WAIT_INFINITE, data);
        }

        /// @brief Sets the internal mode asynchronously.
        /// @param mode The new mode.
        void SetMode(bool mode) {
            AsyncInvoke(shared_from_this(), &Communication::SetModePrivate, m_thread, WAIT_INFINITE, mode);
        }

        /// @brief Gets the internal mode asynchronously.
        /// @return The current mode.
        bool GetMode() {
            return AsyncInvoke(shared_from_this(), &Communication::GetModePrivate, m_thread, WAIT_INFINITE);
        }

    private:
        /// @brief Private implementation of SendData logic.
        /// Executed on the worker thread.
        size_t SendDataPrivate(const std::string& data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cout << data << endl;
            return data.size();
        }
        void SetModePrivate(bool mode) { m_mode = mode; }
        bool GetModePrivate() { return m_mode; }

        bool m_mode = false;
        Thread& m_thread;   ///< The target thread for async execution.
    };

    // -------------------------------------------------------------------------
    // Timeout helpers
    // fast_op completes well within the timeout; slow_op deliberately exceeds it.
    // -------------------------------------------------------------------------
    static size_t fast_op(const std::string& data) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return data.size();
    }

    static size_t slow_op(const std::string& data) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return data.size();
    }

    // -------------------------------------------------------------------------
    // Main Example
    // -------------------------------------------------------------------------
    void AsyncAPIExample()
    {
        // Thread is local. Safe for loops.
        Thread comm_thread("CommunicationThread");
        comm_thread.CreateThread();

        // Create the communication object associated with the thread.
        auto comm = xmake_shared<Communication>(comm_thread);

        // Test member functions
        // These calls originate on the main thread but execute on 'comm_thread'.
        comm->SetMode(true);
        bool mode = comm->GetMode();
        comm->SendData("SendData message");
        comm->SendDataV2("SendDataV2 message");
        comm->SendDataV3("SendDataV3 message");

        // Test free functions (using AsyncInvoke helper)
        // Invoking static methods of AsyncApiHelpers on 'comm_thread'.
        AsyncInvoke(&AsyncApiHelpers::set_mode_private, comm_thread, WAIT_INFINITE, true);
        bool mode2 = AsyncInvoke(&AsyncApiHelpers::get_mode_private, comm_thread, WAIT_INFINITE);
        size_t sent = AsyncInvoke(&AsyncApiHelpers::send_data_private, comm_thread, WAIT_INFINITE, std::string("send_data message"));

        comm_thread.ExitThread();

        // -------------------------------------------------------------------------
        // Async Timeout
        //
        // WAIT_INFINITE blocks until the target thread responds. A finite timeout
        // lets the caller detect a hung or overloaded thread and act on it.
        //
        // Two ways to check whether the call succeeded or timed out:
        //
        //   1. AsyncInvoke() returns std::optional — empty on timeout.
        //      Preferred when you need the return value: has_value() is unambiguous
        //      even when the real return value could be 0 or false.
        //
        //   2. operator()() + IsSuccess() — returns default Ret{} on timeout.
        //      Useful when the delegate is stored and reused across multiple calls.
        // -------------------------------------------------------------------------
        Thread work_thread("WorkThread");
        work_thread.CreateThread();

        constexpr auto kTimeout = std::chrono::milliseconds(50);

        // -- Pattern 1: AsyncInvoke() -> std::optional ---------------------------
        auto delegate = MakeDelegate(&slow_op, work_thread, kTimeout);

        // slow_op sleeps 200ms; timeout is 50ms -> times out
        auto result = delegate.AsyncInvoke(std::string("hello"));
        if (result.has_value())
            cout << "AsyncInvoke slow_op: success, bytes=" << result.value() << "\n";
        else
            cout << "AsyncInvoke slow_op: timed out after 50ms\n";

        // fast_op sleeps 10ms; timeout is 50ms -> succeeds
        auto delegate2 = MakeDelegate(&fast_op, work_thread, kTimeout);
        auto result2 = delegate2.AsyncInvoke(std::string("hello"));
        if (result2.has_value())
            cout << "AsyncInvoke fast_op: success, bytes=" << result2.value() << "\n";
        else
            cout << "AsyncInvoke fast_op: timed out (unexpected)\n";

        // -- Pattern 2: operator()() + IsSuccess() -------------------------------
        auto delegate3 = MakeDelegate(&slow_op, work_thread, kTimeout);
        size_t rv = delegate3(std::string("hello"));
        if (delegate3.IsSuccess())
            cout << "operator() slow_op: success, bytes=" << rv << "\n";
        else
            cout << "operator() slow_op: timed out, default return=" << rv << "\n";

        work_thread.ExitThread();
    }
}
