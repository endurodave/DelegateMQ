#ifndef _SAFETY_SYSTEM_H
#define _SAFETY_SYSTEM_H

#include "DelegateMQ.h"
#include "messages/HeartbeatMsg.h"

namespace cellutron {

/// @brief Top-level system coordinator for the Safety node.
class System {
public:
    static System& GetInstance() {
        static System instance;
        return instance;
    }

    void Initialize();

    Thread& GetThread() { return m_thread; }

private:
    System() = default;
    ~System();

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    void SetupLocalSubscriptions();
    void SetupNetwork();
    void SetupHeartbeat();

    Thread m_thread{"SafetyThread", 50, FullPolicy::DROP};

    dmq::ScopedConnection m_speedConn;
    dmq::ScopedConnection m_heartbeatConn;

    Timer* m_heartbeatTimer = nullptr;
    uint32_t    m_heartbeatCount = 0;
    bool        m_faulted = false;
};

} // namespace cellutron

#endif // _SAFETY_SYSTEM_H
