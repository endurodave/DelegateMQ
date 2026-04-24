#include "Logs.h"
#include "messages/CentrifugeStatusMsg.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "messages/RunStatusMsg.h"
#include "messages/StartProcessMsg.h"
#include "messages/StopProcessMsg.h"
#include "messages/FaultMsg.h"
#include "messages/ActuatorStatusMsg.h"
#include "messages/SensorStatusMsg.h"
#include "Constants.h"
#include <iomanip>
#include <sstream>
#include <chrono>

using namespace dmq;
using namespace dmq::os;
using namespace dmq::util;

namespace cellutron {
namespace util {

Logs::~Logs() {
    Shutdown();
}

void Logs::Initialize() {
    m_file.open("logs.txt", std::ios::out | std::ios::trunc);
    WriteToFile("--- Logging Started ---");

    // Enable DelegateMQ Watchdog
    m_thread.CreateThread(WATCHDOG_TIMEOUT);

    m_startConn = DataBus::Subscribe<StartProcessMsg>(topics::CMD_RUN, [this](StartProcessMsg) {
        WriteToFile("[CMD] START Process Command Sent");
    }, &m_thread);

    m_stopConn = DataBus::Subscribe<StopProcessMsg>(topics::CMD_ABORT, [this](StopProcessMsg) {
        WriteToFile("[CMD] ABORT Process Command Sent");
    }, &m_thread);

    m_speedConn = DataBus::Subscribe<CentrifugeSpeedMsg>(topics::CMD_CENTRIFUGE_SPEED, [this](CentrifugeSpeedMsg msg) {
        std::stringstream ss;
        ss << "[STATUS] Centrifuge Speed: " << msg.rpm << " RPM";
        WriteToFile(ss.str());
    }, &m_thread);

    m_runConn = DataBus::Subscribe<RunStatusMsg>(topics::STATUS_RUN, [this](RunStatusMsg msg) {
        std::string status_text;
        switch (msg.status) {
            case RunStatus::IDLE: status_text = "IDLE"; break;
            case RunStatus::PROCESSING: status_text = "PROCESSING"; break;
            case RunStatus::ABORTING: status_text = "ABORTING"; break;
            case RunStatus::FAULT: status_text = "FAULT"; break;
        }
        WriteToFile("[STATUS] System State: " + status_text);
    }, &m_thread);

    m_faultConn = DataBus::Subscribe<FaultMsg>(topics::FAULT, [this](FaultMsg) {
        WriteToFile("[CRITICAL] FAULT DETECTED BY SAFETY NODE");
    }, &m_thread);

    // Hardware Logging
    m_actuatorConn = DataBus::Subscribe<ActuatorStatusMsg>(topics::STATUS_ACTUATOR, [this](ActuatorStatusMsg msg) {
        std::stringstream ss;
        if (msg.type == ActuatorType::VALVE) {
            ss << "[HW] Valve " << (int)msg.id << " changed to " << (msg.value ? "OPEN" : "CLOSED");
        } else {
            ss << "[HW] Pump " << (int)msg.id << " speed changed to " << (int)msg.value << "%";
        }
        WriteToFile(ss.str());
    }, &m_thread);

    m_sensorConn = DataBus::Subscribe<SensorStatusMsg>(topics::STATUS_SENSOR, [this](SensorStatusMsg msg) {
        std::stringstream ss;
        if (msg.type == SensorType::PRESSURE) {
            ss << "[HW] Pressure Sensor: " << msg.value;
        } else {
            ss << "[HW] Air Detector: " << (msg.value ? "AIR" : "FLUID");
        }
        WriteToFile(ss.str());
    }, &m_thread);
}

void Logs::Shutdown() {
    WriteToFile("--- Logging Shutdown ---");
    m_thread.ExitThread();
    if (m_file.is_open()) {
        m_file.close();
    }
}

void Logs::WriteToFile(const std::string& msg) {
    LockGuard<Mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file << GetTimestamp() << " " << msg << std::endl;
        m_file.flush();
    }
}

std::string Logs::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace util
} // namespace cellutron
