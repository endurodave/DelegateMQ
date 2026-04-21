#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include <cstdint>
#include <chrono>

namespace cellutron {
    // Centrifuge constants
    static constexpr uint16_t MAX_CENTRIFUGE_RPM = 1000;
    static constexpr uint16_t SAFE_CENTRIFUGE_RPM = 900;

    // Heartbeat constants
    static constexpr std::chrono::milliseconds HEARTBEAT_PERIOD{500};
    static constexpr std::chrono::seconds      HEARTBEAT_TIMEOUT{2};
    static constexpr std::chrono::seconds      HEARTBEAT_WARMUP{15};

    namespace topics {
        static const char* const SAFETY_HEARTBEAT     = "sys/heartbeat/safety";
        static const char* const CONTROLLER_HEARTBEAT = "sys/heartbeat/controller";
        static const char* const GUI_HEARTBEAT        = "sys/heartbeat/gui";

        static const char* const PRESSURE_INLET       = "hw/sensor/pressure_inlet";
        static const char* const PRESSURE_OUTLET      = "hw/sensor/pressure_outlet";
        static const char* const AIR_INLET            = "hw/sensor/air_inlet";
        static const char* const AIR_OUTLET           = "hw/sensor/air_outlet";
        static const char* const RPM                  = "hw/sensor/rpm";
        
        static const char* const FAULT                = "cell/fault";
    }

    // Fault codes
    enum FaultCode {
        FAULT_OVERSPEED = 1,
        FAULT_SAFETY_LOST = 2,
        FAULT_AIR_INLET = 3,
        FAULT_BLOCKAGE = 4,
        FAULT_CONTROLLER_LOST = 5,
        FAULT_GUI_LOST = 6,
        FAULT_HEARTBEAT_LOST = 7
    };
}

#endif
