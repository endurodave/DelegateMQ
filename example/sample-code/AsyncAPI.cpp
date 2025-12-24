/// @file
/// @brief Implement an asynchronous API using template helper function.

#include "AsyncAPI.h"
#include "DelegateMQ.h"
#include <iostream>
#include <future>
#include <chrono>

using namespace dmq;
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
            if (m_thread.GetThreadId() != Thread::GetCurrentThreadId())
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
            auto sendDataLambda = [self](const std::string& data) -> bool {
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
    // Main Example
    // -------------------------------------------------------------------------
    void AsyncAPIExample()
    {
        // Thread is local. Safe for loops.
        Thread comm_thread("CommunicationThread");
        comm_thread.CreateThread();

        // Create the communication object associated with the thread.
        auto comm = std::make_shared<Communication>(comm_thread);

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
    }
}
