#ifndef _SYSTEM_H
#define _SYSTEM_H

#include "DelegateMQ.h"
#include "extras/databus/DeadlineSubscription.h"
#include "messages/HeartbeatMsg.h"
#include <memory>

namespace cellutron {

/// @brief Top-level system coordinator for the Controller node.
/// 
/// Handles initialization of all subsystems (Process, Actuators, Sensors),
/// network configuration, and system-wide monitoring (Heartbeat/Watchdog).
class System {
public:
    static System& GetInstance() {
        static System instance;
        return instance;
    }

    /// Initialize the entire controller system.
    void Initialize();

    /// Process periodic system maintenance.
    void Tick();

    /// Get the system thread.
    Thread& GetThread() { return m_thread; }

private:
    System() = default;
    ~System() = default;

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    void SetupLocalSubscriptions();
    void SetupNetwork();
    void SetupHeartbeat();
    void SetupWatchdog();

    Thread m_thread{"SystemThread", 200, FullPolicy::DROP};

    // Connections to the local DataBus
    dmq::ScopedConnection m_startConn;
    dmq::ScopedConnection m_stopConn;
    dmq::ScopedConnection m_faultConn;
    dmq::ScopedConnection m_heartbeatConn;

    // Heartbeat state
    Timer       m_heartbeatTimer;
    uint32_t    m_heartbeatCount = 0;

    // Watchdog to monitor the Safety node's heartbeat
    std::unique_ptr<dmq::DeadlineSubscription<HeartbeatMsg>> m_safetyWatchdog;
    uint32_t m_ticksWaited = 0;
};

} // namespace cellutron

#endif // _SYSTEM_H
