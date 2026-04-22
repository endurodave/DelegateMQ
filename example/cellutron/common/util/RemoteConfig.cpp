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
    dmq::DataBus::RegisterSerializer<StartProcessMsg>(cellutron::topics::CMD_RUN, serStart);
    dmq::DataBus::RegisterSerializer<StopProcessMsg>(cellutron::topics::CMD_ABORT, serStop);
    dmq::DataBus::RegisterSerializer<CentrifugeSpeedMsg>(cellutron::topics::CMD_CENTRIFUGE_SPEED, serSpeed);
    dmq::DataBus::RegisterSerializer<CentrifugeStatusMsg>(cellutron::topics::STATUS_CENTRIFUGE, serStatus);
    dmq::DataBus::RegisterSerializer<RunStatusMsg>(cellutron::topics::STATUS_RUN, serRun);
    dmq::DataBus::RegisterSerializer<FaultMsg>(cellutron::topics::FAULT, serFault);
    dmq::DataBus::RegisterSerializer<ActuatorStatusMsg>(cellutron::topics::STATUS_ACTUATOR, serActuator);
    dmq::DataBus::RegisterSerializer<SensorStatusMsg>(cellutron::topics::STATUS_SENSOR, serSensor);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::SAFETY_HEARTBEAT, serHeartbeat);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::CONTROLLER_HEARTBEAT, serHeartbeat);
    dmq::DataBus::RegisterSerializer<HeartbeatMsg>(cellutron::topics::GUI_HEARTBEAT, serHeartbeat);
}

void RegisterStringifiers() {
    dmq::DataBus::RegisterStringifier<StartProcessMsg>(cellutron::topics::CMD_RUN, [](const StartProcessMsg&) {
        return "START";
    });

    dmq::DataBus::RegisterStringifier<StopProcessMsg>(cellutron::topics::CMD_ABORT, [](const StopProcessMsg&) {
        return "ABORT";
    });

    dmq::DataBus::RegisterStringifier<CentrifugeSpeedMsg>(cellutron::topics::CMD_CENTRIFUGE_SPEED, [](const CentrifugeSpeedMsg& msg) {
        return std::to_string(msg.rpm) + " RPM";
    });

    dmq::DataBus::RegisterStringifier<CentrifugeStatusMsg>(cellutron::topics::STATUS_CENTRIFUGE, [](const CentrifugeStatusMsg& msg) {
        return std::to_string(msg.rpm) + " RPM";
    });

    dmq::DataBus::RegisterStringifier<RunStatusMsg>(cellutron::topics::STATUS_RUN, [](const RunStatusMsg& msg) {
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

    dmq::DataBus::RegisterStringifier<ActuatorStatusMsg>(cellutron::topics::STATUS_ACTUATOR, [](const ActuatorStatusMsg& msg) {
        std::string type = (msg.type == ActuatorType::VALVE) ? "VALVE" : "PUMP";
        return type + " " + std::to_string(msg.id) + ": " + std::to_string(msg.value);
    });

    dmq::DataBus::RegisterStringifier<SensorStatusMsg>(cellutron::topics::STATUS_SENSOR, [](const SensorStatusMsg& msg) {
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
