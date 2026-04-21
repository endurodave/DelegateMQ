#ifndef _ACTUATORS_H
#define _ACTUATORS_H

#include "DelegateMQ.h"
#include "Centrifuge.h"
#include "Valve.h"
#include "Pump.h"
#include <map>

/// @brief Singleton class for controlling hardware actuators.
class Actuators {
public:
    /// Signals for actuator status changes.
    dmq::Signal<void(int id, bool open)> OnValveChanged;
    dmq::Signal<void(int id, int speed)> OnPumpChanged;

    static Actuators& GetInstance() {
        static Actuators instance;
        return instance;
    }

    void Initialize();
    void Shutdown();

    /// Set a valve state (Synchronous Blocking call)
    int SetValve(int id, bool open);

    /// Set a pump speed (Synchronous Blocking call)
    int SetPump(int id, int speed);

    /// Get the centrifuge actuator.
    hw::Centrifuge& GetCentrifuge() { return m_centrifuge; }

    /// Get a valve by ID.
    hw::Valve& GetValve(int id) { return m_valves.at(id); }

    /// Get a pump by ID.
    hw::Pump& GetPump(int id) { return m_pumps.at(id); }

    /// Get the actuator thread.
    Thread& GetThread() { return m_thread; }

private:
    Actuators() = default;
    ~Actuators();

    Actuators(const Actuators&) = delete;
    Actuators& operator=(const Actuators&) = delete;

    int InternalSetValve(int id, bool open);
    int InternalSetPump(int id, int speed);

    void HandleValveChanged(int id, bool open);
    void HandlePumpChanged(int id, uint8_t speed);

    // Use standardized thread name for Active Object subsystem
    Thread m_thread{"ActuatorThread", 50, FullPolicy::DROP};
    hw::Centrifuge m_centrifuge;

    std::map<int, hw::Valve> m_valves;
    std::map<int, hw::Pump>  m_pumps;

    std::map<int, dmq::ScopedConnection> m_valveConns;
    std::map<int, dmq::ScopedConnection> m_pumpConns;
};

#endif
