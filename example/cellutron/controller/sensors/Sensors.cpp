#include "Sensors.h"
#include "messages/SensorStatusMsg.h"
#include "util/Constants.h"
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

using namespace dmq;

namespace cellutron {
namespace sensors {

Sensors::~Sensors() {
    Shutdown();
}

void Sensors::Initialize() {
    // Enable DelegateMQ Watchdog (2 second timeout)
    m_thread.SetThreadPriority(PRIORITY_HARDWARE);
    m_thread.CreateThread(WATCHDOG_TIMEOUT);
    printf("Sensors: Subsystem initialized.\n");
}

void Sensors::Shutdown() {
    m_thread.ExitThread();
}

int Sensors::GetPressure() {
    return dmq::MakeDelegate(this, &Sensors::InternalGetPressure, m_thread)();
}

bool Sensors::IsAirInLine() {
    return dmq::MakeDelegate(this, &Sensors::InternalIsAirInLine, m_thread)();
}

int Sensors::InternalGetPressure() {
    int pressure = 0;
    DataBus::Publish<SensorStatusMsg>(topics::STATUS_SENSOR, { SensorType::PRESSURE, (int16_t)pressure });
    return pressure; 
}

bool Sensors::InternalIsAirInLine() {
    bool air = false;
    DataBus::Publish<SensorStatusMsg>(topics::STATUS_SENSOR, { SensorType::AIR_IN_LINE, (int16_t)(air ? 1 : 0) });
    return air;
}

} // namespace sensors
} // namespace cellutron
