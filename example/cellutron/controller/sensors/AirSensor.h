#ifndef _AIR_SENSOR_H
#define _AIR_SENSOR_H

#include "DelegateMQ.h"
#include "messages/SensorStatusMsg.h"
#include "util/Constants.h"

/// @brief Simulates two air-in-line detectors: A1 (inlet) and A2 (outlet).
///
/// Both sensors always report fluid present (no air). Poll() is called every
/// 50ms by the Sensors manager on the SensorThread.
class AirSensor
{
public:
    void Poll()
    {
        dmq::DataBus::Publish<SensorStatusMsg>(
            cellutron::topics::AIR_INLET,
            { SensorType::AIR_IN_LINE, m_inletAir ? int16_t(1) : int16_t(0) });

        dmq::DataBus::Publish<SensorStatusMsg>(
            cellutron::topics::AIR_OUTLET,
            { SensorType::AIR_IN_LINE, m_outletAir ? int16_t(1) : int16_t(0) });
    }

    bool IsInletAir()  const { return m_inletAir; }
    bool IsOutletAir() const { return m_outletAir; }

private:
    bool m_inletAir  = false;
    bool m_outletAir = false;
};

#endif // _AIR_SENSOR_H
