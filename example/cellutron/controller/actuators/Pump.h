#ifndef _HW_PUMP_H
#define _HW_PUMP_H

#include <cstdint>
#include <cstdio>

namespace hw {

/// @brief Represents a single peristaltic pump.
class Pump {
public:
    Pump(uint8_t id) : m_id(id) {}

    /// Set speed (0-100%).
    void SetSpeed(uint8_t speed) {
        m_speed = speed;
        printf("[Pump %d] -> %d%%\n", m_id, m_speed);
    }

    uint8_t GetSpeed() const { return m_speed; }
    uint8_t GetId()    const { return m_id; }

private:
    uint8_t m_id;
    uint8_t m_speed = 0;
};

} // namespace hw

#endif // _HW_PUMP_H
