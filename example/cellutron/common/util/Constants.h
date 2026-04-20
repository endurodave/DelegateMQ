#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include <cstdint>

namespace cellutron {
    // Centrifuge constants
    static constexpr uint16_t MAX_CENTRIFUGE_RPM = 1000;
    static constexpr uint16_t SAFE_CENTRIFUGE_RPM = 900;

    namespace topics {
        static const char* const SAFETY_HEARTBEAT     = "sys/heartbeat/safety";
        static const char* const CONTROLLER_HEARTBEAT = "sys/heartbeat/controller";
        static const char* const PRESSURE_INLET       = "hw/sensor/pressure_inlet";
        static const char* const PRESSURE_OUTLET      = "hw/sensor/pressure_outlet";
        static const char* const AIR_INLET            = "hw/sensor/air_inlet";
        static const char* const AIR_OUTLET           = "hw/sensor/air_outlet";
        static const char* const RPM                  = "hw/sensor/rpm";
    }

    // Fault codes
    enum FaultCode {
        FAULT_OVERSPEED = 1,
        FAULT_SAFETY_LOST = 2,
        FAULT_AIR_INLET = 3,
        FAULT_BLOCKAGE = 4,
        FAULT_CONTROLLER_LOST = 5
    };
}

#endif
