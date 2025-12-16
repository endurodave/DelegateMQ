/// @brief Implement the safe timer test to prevent a latent callback 
/// on a dead object. 
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
        SafeTimer() : m_thread("SafeTimer Thread") { }

        void Init() {
            m_thread.CreateThread();

            // Bind the callback with a shared reference. This ensures the object remains
            // valid/alive when the timer fires, preventing a crash on a dangling pointer.
            m_timer.Expired = MakeDelegate(shared_from_this(), &SafeTimer::OnTimer, m_thread);
            m_timer.Start(std::chrono::milliseconds(100));
        }

        // Unregister in Term to be clean
        void Term() {
            m_timer.Stop();
            m_timer.Expired.Clear();

            m_thread.ExitThread();
        }

        ~SafeTimer() {
            // Even if Shutdown() wasn't called, the shared delegate prevents a crash
            // if a stray timer event is still pending in workerThread.
        }

    private:
        void OnTimer() {
            std::cout << "Timer fired safely!" << std::endl;
        }

        Timer m_timer;
        Thread m_thread;
    };

    void SafeTimerExample() {
        auto safeTimer = std::make_shared<SafeTimer>();
        safeTimer->Init();

        this_thread::sleep_for(chrono::milliseconds(500));

        safeTimer->Term();
    }
}