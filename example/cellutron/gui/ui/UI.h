#ifndef _UI_H
#define _UI_H

#include "DelegateMQ.h"
#include "messages/RunStatusMsg.h"
#include "Constants.h"
#include <atomic>
#include <string>
#include <vector>
#include <chrono>

namespace cellutron {
namespace gui {

/// @brief Singleton class for the User Interface.
class UI {
public:
    static UI& GetInstance() {
        static UI instance;
        return instance;
    }

    /// Start the UI event loop (blocking).
    void Start();

    /// Shutdown the UI.
    void Shutdown();

private:
    UI() = default;
    ~UI();

    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void AddLog(const std::string& msg);

    // --- State Members ---
    std::atomic<uint16_t> m_currentRpm{0};
    std::atomic<int16_t> m_currentPumpSpeed{0};
    std::atomic<RunStatus> m_runStatus{RunStatus::IDLE};
    dmq::Mutex m_uiMutex;
    std::vector<std::string> m_logs;
    std::chrono::steady_clock::time_point m_lastClickTime = std::chrono::steady_clock::now();

    // Use standardized thread name for Active Object subsystem
    Thread m_thread{"UIThread", 50, FullPolicy::DROP};
};

} // namespace gui
} // namespace cellutron

#endif
