/// @brief Implement the safe timer test using RAII ScopedConnections.
/// This prevents latent callbacks on dead objects without manual Stop() calls.
/// 
/// See Object Lifetime and Async Delegates in DETAILS.md for more information.

#include "SafeTimer.h"
#include "DelegateMQ.h"

using namespace dmq;
using namespace std;

namespace Example
{
    // The SafeTimer class safely creates a timer and registers/unregisters
    // while preventing any pending timer callbacks on a deleted object. 
    class SafeTimer : public std::enable_shared_from_this<SafeTimer>
    {
    public:
        SafeTimer() : m_thread("SafeTimer Thread") {}

        void Init() {
            m_thread.CreateThread();

            // Bind the callback using Connect().
            // We use shared_from_this() to ensure the object stays alive if the callback is queued.
            // We store the connection in m_timerConn so it automatically disconnects on destruction.
            m_timerConn = m_timer.Expired->Connect(
                MakeDelegate(shared_from_this(), &SafeTimer::OnTimer, m_thread)
            );

            m_timer.Start(std::chrono::milliseconds(100));
        }

        // Unregister in Term to be clean
        void Term() {
            m_timer.Stop();

            // Manual disconnect (optional, since destructor handles it, but good for deterministic cleanup)
            m_timerConn.Disconnect();

            m_thread.ExitThread();
        }

        ~SafeTimer() {
            // m_timerConn destructor runs here.
            // It guarantees the timer signal is disconnected immediately.
        }

    private:
        void OnTimer() {
            std::cout << "Timer fired safely!" << std::endl;
        }

        Timer m_timer;
        Thread m_thread;

        // RAII Connection Handle
        // When this object dies, the subscription to the timer is automatically removed.
        dmq::ScopedConnection m_timerConn;
    };

    void SafeTimerExample() {
        auto safeTimer = std::make_shared<SafeTimer>();
        safeTimer->Init();

        this_thread::sleep_for(chrono::milliseconds(500));

        safeTimer->Term();

        // When safeTimer goes out of scope here, everything cleans up automatically.
    }
}