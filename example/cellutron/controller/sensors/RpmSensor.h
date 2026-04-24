#ifndef _RPM_SENSOR_H
#define _RPM_SENSOR_H

#include "DelegateMQ.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "util/Constants.h"
#include <atomic>

namespace cellutron {
namespace sensors {

/// @brief Simulates a centrifuge tachometer (RPM sensor).
///
/// Subscribes to the commanded centrifuge speed topic and re-publishes it on
/// the hardware sensor topic every 50ms, tracking the ramp profile.
class RpmSensor
{
public:
    void Initialize(dmq::IThread& thread)
    {
        m_conn = dmq::databus::DataBus::Subscribe<CentrifugeSpeedMsg>(
            topics::CMD_CENTRIFUGE_SPEED,
            [this](CentrifugeSpeedMsg msg) {
                m_currentRpm.store(msg.rpm);
            },
            &thread);
    }

    void Poll()
    {
        dmq::databus::DataBus::Publish<CentrifugeSpeedMsg>(
            cellutron::topics::RPM,
            { m_currentRpm.load() });
    }

    uint16_t GetRpm() const { return m_currentRpm.load(); }

private:
    std::atomic<uint16_t> m_currentRpm{0};
    dmq::ScopedConnection m_conn;
};

} // namespace sensors
} // namespace cellutron

#endif // _RPM_SENSOR_H
