#include "Actuators.h"
#include "messages/ActuatorStatusMsg.h"
#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

using namespace dmq;

Actuators::~Actuators() {
    Shutdown();
}

void Actuators::Initialize() {
    // Enable DelegateMQ Watchdog (2 second timeout)
    m_thread.CreateThread(std::chrono::seconds(2));
    printf("Actuators: Subsystem initialized.\n");
}

void Actuators::Shutdown() {
    m_thread.ExitThread();
}

int Actuators::SetValve(int id, bool open) {
    return dmq::MakeDelegate(this, &Actuators::InternalSetValve, m_thread)(id, open);
}

int Actuators::SetPump(int id, int speed) {
    return dmq::MakeDelegate(this, &Actuators::InternalSetPump, m_thread)(id, speed);
}

int Actuators::InternalSetValve(int id, bool open) {
    printf("SNA [Thread: %s]: Setting Valve %d to %s...\n", 
           m_thread.GetThreadName().c_str(), id, open ? "OPEN" : "CLOSED");
    
    DataBus::Publish<ActuatorStatusMsg>("hw/status/actuator", { ActuatorType::VALVE, (uint8_t)id, (uint8_t)(open ? 1 : 0) });
    
    vTaskDelay(pdMS_TO_TICKS(100));
    return 0; 
}

int Actuators::InternalSetPump(int id, int speed) {
    printf("SNA [Thread: %s]: Setting Pump %d speed to %d%%...\n", 
           m_thread.GetThreadName().c_str(), id, speed);
    
    DataBus::Publish<ActuatorStatusMsg>("hw/status/actuator", { ActuatorType::PUMP, (uint8_t)id, (uint8_t)speed });
    
    vTaskDelay(pdMS_TO_TICKS(100));
    return 0;
}
