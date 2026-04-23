#ifndef _SAFETY_SYSTEM_H
#define _SAFETY_SYSTEM_H

#include "DelegateMQ.h"
#include "util/Heartbeat.h"

namespace cellutron {

/// @brief Top-level system coordinator for the Safety node.
class System {
public:
    static System& GetInstance() {
        static System instance;
        return instance;
    }

    void Initialize();
    void Tick();

    Thread& GetThread() { return m_thread; }

private:
    System() : m_heartbeat("Safety", topics::SAFETY_HEARTBEAT, m_thread) {}
    ~System() = default;

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    void SetupLocalSubscriptions();
    void SetupNetwork();
    void SetupWatchdog();

    Thread m_thread{"SystemThread", 50, FullPolicy::DROP};

    dmq::ScopedConnection m_speedConn;
    dmq::ScopedConnection m_faultConn;

    util::Heartbeat m_heartbeat;
    bool      m_faulted = false;
};

} // namespace cellutron

#endif // _SAFETY_SYSTEM_H
