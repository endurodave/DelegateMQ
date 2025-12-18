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
    class Communication : public std::enable_shared_from_this<Communication>
    {
    public:
        Communication(Thread& t) : m_thread(t) {}

        size_t SendData(const std::string& data) {
            return AsyncInvoke(shared_from_this(), &Communication::SendDataPrivate, m_thread, WAIT_INFINITE, data);
        }

        size_t SendDataV2(const std::string& data) {
            if (m_thread.GetThreadId() != Thread::GetCurrentThreadId())
                return MakeDelegate(shared_from_this(), &Communication::SendDataV2, m_thread, WAIT_INFINITE)(data);
            cout << data << endl;
            return data.size();
        }

        size_t SendDataV3(const std::string& data) {
            auto self = shared_from_this();
            auto sendDataLambda = [self](const std::string& data) -> bool {
                cout << data << endl;
                return data.size();
                };
            return AsyncInvoke(sendDataLambda, m_thread, WAIT_INFINITE, data);
        }

        void SetMode(bool mode) {
            AsyncInvoke(shared_from_this(), &Communication::SetModePrivate, m_thread, WAIT_INFINITE, mode);
        }

        bool GetMode() {
            return AsyncInvoke(shared_from_this(), &Communication::GetModePrivate, m_thread, WAIT_INFINITE);
        }

    private:
        size_t SendDataPrivate(const std::string& data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cout << data << endl;
            return data.size();
        }
        void SetModePrivate(bool mode) { m_mode = mode; }
        bool GetModePrivate() { return m_mode; }

        bool m_mode = false;
        Thread& m_thread;
    };

    // -------------------------------------------------------------------------
    // Main Example
    // -------------------------------------------------------------------------
    void AsyncAPIExample()
    {
        // Thread is local. Safe for loops.
        Thread comm_thread("CommunicationThread");
        comm_thread.CreateThread();

        auto comm = std::make_shared<Communication>(comm_thread);

        // Test member functions
        comm->SetMode(true);
        bool mode = comm->GetMode();
        comm->SendData("SendData message");
        comm->SendDataV2("SendDataV2 message");
        comm->SendDataV3("SendDataV3 message");

        // Test free functions (using AsyncInvoke helper)
        AsyncInvoke(&AsyncApiHelpers::set_mode_private, comm_thread, WAIT_INFINITE, true);
        bool mode2 = AsyncInvoke(&AsyncApiHelpers::get_mode_private, comm_thread, WAIT_INFINITE);
        size_t sent = AsyncInvoke(&AsyncApiHelpers::send_data_private, comm_thread, WAIT_INFINITE, std::string("send_data message"));

        comm_thread.ExitThread();
    }
}