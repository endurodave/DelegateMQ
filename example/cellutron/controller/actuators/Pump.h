#ifndef _PUMP_H
#define _PUMP_H

#include "DelegateMQ.h"
#include "messages/ActuatorStatusMsg.h"
#include <cstdint>
#include <cstdio>

namespace cellutron {
namespace actuators {

/// @brief Represents a single peristaltic pump.
class Pump {
public:
    /// Signal emitted when pump speed changes.
    /// int: The pump ID.
    /// uint8_t: The new speed (0-100).
    dmq::Signal<void(int, uint8_t)> OnSpeedChanged;

    Pump(uint8_t id) : m_id(id) {}

    /// Set speed (0-100%).
    void SetSpeed(uint8_t speed) {
        m_speed = speed;
        printf("[Pump %d] -> %d%%\n", m_id, m_speed);
        dmq::DataBus::Publish<ActuatorStatusMsg>(
            "hw/status/actuator",
            { ActuatorType::PUMP, m_id, m_speed });
        OnSpeedChanged(m_id, m_speed);
    }

    uint8_t GetSpeed() const { return m_speed; }
    uint8_t GetId()    const { return m_id; }

private:
    uint8_t m_id;
    uint8_t m_speed = 0;
};

} // namespace actuators
} // namespace cellutron

#endif // _PUMP_H
