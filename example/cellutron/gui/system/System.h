#ifndef _GUI_SYSTEM_H
#define _GUI_SYSTEM_H

#include "DelegateMQ.h"
#include <thread>
#include <atomic>

namespace cellutron {

/// @brief Top-level system coordinator for the GUI node.
class System {
public:
    static System& GetInstance() {
        static System instance;
        return instance;
    }

    void Initialize();
    void Shutdown();

private:
    System() = default;
    ~System();

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    void SetupNetwork();
    void StartTimerThread();

    Thread m_timerThread{"TimerThread", 200, FullPolicy::DROP};
    
    std::atomic<bool> m_timerRunning{false};
    std::thread m_backgroundTimer;
};

} // namespace cellutron

#endif // _GUI_SYSTEM_H
