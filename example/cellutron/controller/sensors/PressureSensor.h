#ifndef _PRESSURE_SENSOR_H
#define _PRESSURE_SENSOR_H

#include "DelegateMQ.h"
#include "messages/SensorStatusMsg.h"
#include "util/Constants.h"

namespace cellutron {
namespace sensors {

/// @brief Simulates two pressure sensors: P1 (inlet) and P2 (outlet).
///
/// Both sensors always report a nominal "good" value. Poll() is called every
/// 50ms by the Sensors manager on the SensorThread.
class PressureSensor
{
public:
    void Poll()
    {
        dmq::databus::DataBus::Publish<SensorStatusMsg>(
            cellutron::topics::PRESSURE_INLET,
            { SensorType::PRESSURE, m_inletKPa });

        dmq::databus::DataBus::Publish<SensorStatusMsg>(
            cellutron::topics::PRESSURE_OUTLET,
            { SensorType::PRESSURE, m_outletKPa });
    }

    int16_t GetInlet()  const { return m_inletKPa; }
    int16_t GetOutlet() const { return m_outletKPa; }

private:
    int16_t m_inletKPa  = 0;
    int16_t m_outletKPa = 0;
};

} // namespace sensors
} // namespace cellutron

#endif // _PRESSURE_SENSOR_H
