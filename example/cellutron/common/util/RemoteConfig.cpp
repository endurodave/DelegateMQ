#include "RemoteConfig.h"

Serializer<void(StartProcessMsg)>    serStart;
Serializer<void(StopProcessMsg)>     serStop;
Serializer<void(CentrifugeSpeedMsg)> serSpeed;
Serializer<void(CentrifugeStatusMsg)> serStatus;
Serializer<void(RunStatusMsg)>       serRun;
Serializer<void(FaultMsg)>           serFault;
Serializer<void(ActuatorStatusMsg)>  serActuator;
Serializer<void(SensorStatusMsg)>    serSensor;
Serializer<void(HeartbeatMsg)>       serHeartbeat;

void RegisterStringifiers() {
    dmq::DataBus::RegisterStringifier<StartProcessMsg>("cell/cmd/run", [](const StartProcessMsg&) {
        return "START";
    });

    dmq::DataBus::RegisterStringifier<StopProcessMsg>("cell/cmd/abort", [](const StopProcessMsg&) {
        return "ABORT";
    });

    dmq::DataBus::RegisterStringifier<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", [](const CentrifugeSpeedMsg& msg) {
        return std::to_string(msg.rpm) + " RPM";
    });

    dmq::DataBus::RegisterStringifier<CentrifugeStatusMsg>("cell/status/centrifuge", [](const CentrifugeStatusMsg& msg) {
        return std::to_string(msg.rpm) + " RPM";
    });

    dmq::DataBus::RegisterStringifier<RunStatusMsg>("cell/status/run", [](const RunStatusMsg& msg) {
        switch(msg.status) {
            case RunStatus::IDLE: return "IDLE";
            case RunStatus::PROCESSING: return "PROCESSING";
            case RunStatus::ABORTING: return "ABORTING";
            case RunStatus::FAULT: return "FAULT";
            default: return "UNKNOWN";
        }
    });

    dmq::DataBus::RegisterStringifier<FaultMsg>("cell/fault", [](const FaultMsg& msg) {
        return "FAULT CODE: " + std::to_string(msg.faultCode);
    });

    dmq::DataBus::RegisterStringifier<ActuatorStatusMsg>("hw/status/actuator", [](const ActuatorStatusMsg& msg) {
        std::string type = (msg.type == ActuatorType::VALVE) ? "VALVE" : "PUMP";
        return type + " " + std::to_string(msg.id) + ": " + std::to_string(msg.value);
    });

    dmq::DataBus::RegisterStringifier<SensorStatusMsg>("hw/status/sensor", [](const SensorStatusMsg& msg) {
        std::string type = (msg.type == SensorType::PRESSURE) ? "PRESSURE" : "AIR";
        return type + ": " + std::to_string(msg.value);
    });

    dmq::DataBus::RegisterStringifier<HeartbeatMsg>("sys/heartbeat/safety", [](const HeartbeatMsg& msg) {
        return "SAFETY HB: " + std::to_string(msg.counter);
    });

    dmq::DataBus::RegisterStringifier<HeartbeatMsg>("sys/heartbeat/controller", [](const HeartbeatMsg& msg) {
        return "CONTROLLER HB: " + std::to_string(msg.counter);
    });

    // Register stringifiers for topic strings that are used in code
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>("hw/sensor/pressure_inlet", [](const SensorStatusMsg& msg) {
        return "P1: " + std::to_string(msg.value);
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>("hw/sensor/pressure_outlet", [](const SensorStatusMsg& msg) {
        return "P2: " + std::to_string(msg.value);
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>("hw/sensor/air_inlet", [](const SensorStatusMsg& msg) {
        return "A1: " + std::string(msg.value ? "AIR" : "FLUID");
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>("hw/sensor/air_outlet", [](const SensorStatusMsg& msg) {
        return "A2: " + std::string(msg.value ? "AIR" : "FLUID");
    });
    dmq::DataBus::RegisterStringifier<CentrifugeSpeedMsg>("hw/sensor/rpm", [](const CentrifugeSpeedMsg& msg) {
        return "RPM: " + std::to_string(msg.rpm);
    });
}
