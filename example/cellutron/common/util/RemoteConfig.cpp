#include "RemoteConfig.h"
#include "util/Constants.h"

Serializer<void(StartProcessMsg)>    serStart;
Serializer<void(StopProcessMsg)>     serStop;
Serializer<void(CentrifugeSpeedMsg)> serSpeed;
Serializer<void(CentrifugeStatusMsg)> serStatus;
Serializer<void(RunStatusMsg)>       serRun;
Serializer<void(FaultMsg)>           serFault;
Serializer<void(ActuatorStatusMsg)>  serActuator;
Serializer<void(SensorStatusMsg)>    serSensor;
Serializer<void(HeartbeatMsg)>       serHeartbeat;

void RegisterSerializers() {
    dmq::DataBus::RegisterSerializer<StartProcessMsg>("cell/cmd/run", serStart);
    dmq::DataBus::RegisterSerializer<StopProcessMsg>("cell/cmd/abort", serStop);
    dmq::DataBus::RegisterSerializer<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", serSpeed);
    dmq::DataBus::RegisterSerializer<CentrifugeStatusMsg>("cell/status/centrifuge", serStatus);
    dmq::DataBus::RegisterSerializer<RunStatusMsg>("cell/status/run", serRun);
    dmq::DataBus::RegisterSerializer<FaultMsg>(cellutron::topics::FAULT, serFault);
    dmq::DataBus::RegisterSerializer<ActuatorStatusMsg>("hw/status/actuator", serActuator);
    dmq::DataBus::RegisterSerializer<SensorStatusMsg>("hw/status/sensor", serSensor);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::SAFETY_HEARTBEAT, serHeartbeat);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::CONTROLLER_HEARTBEAT, serHeartbeat);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::GUI_HEARTBEAT, serHeartbeat);
}

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

    dmq::DataBus::RegisterStringifier<FaultMsg>(cellutron::topics::FAULT, [](const FaultMsg& msg) {
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

    dmq::DataBus::RegisterStringifier<HeartbeatMsg>(cellutron::topics::SAFETY_HEARTBEAT, [](const HeartbeatMsg& msg) {
        return "SAFETY HB: " + std::to_string(msg.counter);
    });

    dmq::DataBus::RegisterStringifier<HeartbeatMsg>(cellutron::topics::CONTROLLER_HEARTBEAT, [](const HeartbeatMsg& msg) {
        return "CONTROLLER HB: " + std::to_string(msg.counter);
    });

    dmq::DataBus::RegisterStringifier<HeartbeatMsg>(cellutron::topics::GUI_HEARTBEAT, [](const HeartbeatMsg& msg) {
        return "GUI HB: " + std::to_string(msg.counter);
    });

    // Register stringifiers for topic strings that are used in code
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>(cellutron::topics::PRESSURE_INLET, [](const SensorStatusMsg& msg) {
        return "P1: " + std::to_string(msg.value);
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>(cellutron::topics::PRESSURE_OUTLET, [](const SensorStatusMsg& msg) {
        return "P2: " + std::to_string(msg.value);
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>(cellutron::topics::AIR_INLET, [](const SensorStatusMsg& msg) {
        return "A1: " + std::string(msg.value ? "AIR" : "FLUID");
    });
    dmq::DataBus::RegisterStringifier<SensorStatusMsg>(cellutron::topics::AIR_OUTLET, [](const SensorStatusMsg& msg) {
        return "A2: " + std::string(msg.value ? "AIR" : "FLUID");
    });
    dmq::DataBus::RegisterStringifier<CentrifugeSpeedMsg>(cellutron::topics::RPM, [](const CentrifugeSpeedMsg& msg) {
        return "RPM: " + std::to_string(msg.rpm);
    });
}
