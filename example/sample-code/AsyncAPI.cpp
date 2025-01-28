/// @file
/// @brief Implement an asynchronous API using template helper function.
/// See Async-SQL repo that wraps sqlite library with an asynchronous API.
/// https://github.com/endurodave/Async-SQLite

#include "AsyncAPI.h"
#include "DelegateLib.h"
#include <iostream>
#include <future>
#include <chrono>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    static Thread comm_thread("CommunicationThread");
    static bool _mode = false;

    // Private implementation function only called on comm_thread.
    // Assume this function is not thread-safe and a random std::async 
    // thread from the pool is unacceptable and causes cross-threading.
    static size_t send_data_private(const std::string& data)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
        cout << data << endl;
        return data.size();  // Return the 'bytes_sent' sent result
    }

    // Public function to send data. Thread-safe callable by any thread.
    static size_t send_data(const std::string& data) {
        return AsyncInvoke(send_data_private, comm_thread, WAIT_INFINITE, data);
    }

    static void set_mode_private(bool mode) {
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
        _mode = mode;
    }

    static void set_mode(bool mode) {
        AsyncInvoke(set_mode_private, comm_thread, WAIT_INFINITE, mode);
    }

    static bool get_mode_private() {
        return _mode;
    }

    static bool get_mode() {
        return AsyncInvoke(get_mode_private, comm_thread, WAIT_INFINITE);
    }

    // Communication class with a fully asynchronous public API
    class Communication
    {
    public:
        // All public functions async invoke the function call onto comm_thread

        // Async API calls the private implementation
        size_t SendData(const std::string& data) {
            return AsyncInvoke(this, &Communication::SendDataPrivate, comm_thread, WAIT_INFINITE, data);
        }

        // Alternate async API implementation style all within the public function
        size_t SendDataV2(const std::string& data) {
            if (comm_thread.GetThreadId() != Thread::GetCurrentThreadId())
                return MakeDelegate(this, &Communication::SendDataV2, comm_thread, WAIT_INFINITE)(data);

            std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
            cout << data << endl;
            return data.size();  // Return the 'bytes_sent' sent result
        }

        void SetMode(bool mode) {
            AsyncInvoke(this, &Communication::SetModePrivate, comm_thread, WAIT_INFINITE, mode);
        }

        bool GetMode() {
            return AsyncInvoke(this, &Communication::GetModePrivate, comm_thread, WAIT_INFINITE);
        }

    private:
        // All private functions all only called from comm_thread 

        size_t SendDataPrivate(const std::string& data) {
            std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
            cout << data << endl;
            return data.size();  // Return the 'bytes_sent' sent result
        }

        void SetModePrivate(bool mode) { m_mode = mode; }
        bool GetModePrivate() { return m_mode; }

        bool m_mode = false;
    };

    void AsyncAPIExample()
    {
        comm_thread.CreateThread();

        Communication comm;

        // Test async invoke of member functions
        comm.SetMode(true);
        bool mode = comm.GetMode();
        comm.SendData("SendData message");
        comm.SendDataV2("SendDataV2 message");

        // Test async invoke of free functions
        set_mode(true);
        bool mode2 = get_mode();
        size_t sent_bytes = send_data("send_date message");

        comm_thread.ExitThread();
    }
}
