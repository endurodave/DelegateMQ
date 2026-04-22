#ifndef _SENSORS_H
#define _SENSORS_H

#include "DelegateMQ.h"

namespace cellutron {
namespace sensors {

/// @brief Singleton class for monitoring hardware sensors.
class Sensors {
public:
    static Sensors& GetInstance() {
        static Sensors instance;
        return instance;
    }

    void Initialize();
    void Shutdown();

    /// Get current pressure (Blocking call)
    int GetPressure();

    /// Check if air is detected in the line (Blocking call)
    bool IsAirInLine();

private:
    Sensors() = default;
    ~Sensors();

    Sensors(const Sensors&) = delete;
    Sensors& operator=(const Sensors&) = delete;

    int InternalGetPressure();
    bool InternalIsAirInLine();

    // Use standardized thread name for Active Object subsystem
    Thread m_thread{"SensorsThread"};
};

} // namespace sensors
} // namespace cellutron

#endif
