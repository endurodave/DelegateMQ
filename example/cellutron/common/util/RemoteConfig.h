#ifndef REMOTE_CONFIG_H
#define REMOTE_CONFIG_H

#include "DelegateMQ.h"
#include "port/serialize/serialize/Serializer.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "messages/CentrifugeStatusMsg.h"
#include "messages/RunStatusMsg.h"
#include "messages/StartProcessMsg.h"
#include "messages/StopProcessMsg.h"
#include "messages/FaultMsg.h"
#include "messages/ActuatorStatusMsg.h"
#include "messages/SensorStatusMsg.h"
#include "messages/HeartbeatMsg.h"

namespace cellutron {

// Remote IDs
static constexpr dmq::DelegateRemoteId RID_START_PROCESS     = 100;
static constexpr dmq::DelegateRemoteId RID_STOP_PROCESS      = 104;
static constexpr dmq::DelegateRemoteId RID_CENTRIFUGE_SPEED   = 101;
static constexpr dmq::DelegateRemoteId RID_CENTRIFUGE_STATUS  = 102;
static constexpr dmq::DelegateRemoteId RID_RUN_STATUS         = 103;
static constexpr dmq::DelegateRemoteId RID_FAULT_EVENT        = 105;
static constexpr dmq::DelegateRemoteId RID_ACTUATOR_STATUS    = 106;
static constexpr dmq::DelegateRemoteId RID_SENSOR_STATUS      = 107;
static constexpr dmq::DelegateRemoteId RID_SAFETY_HB           = 108;
static constexpr dmq::DelegateRemoteId RID_CONTROLLER_HB       = 109;
static constexpr dmq::DelegateRemoteId RID_GUI_HB              = 110;

// Shared Serializers
extern dmq::serialization::serializer::Serializer<void(StartProcessMsg)>    serStart;
extern dmq::serialization::serializer::Serializer<void(StopProcessMsg)>     serStop;
extern dmq::serialization::serializer::Serializer<void(CentrifugeSpeedMsg)> serSpeed;
extern dmq::serialization::serializer::Serializer<void(CentrifugeStatusMsg)> serStatus;
extern dmq::serialization::serializer::Serializer<void(RunStatusMsg)>       serRun;
extern dmq::serialization::serializer::Serializer<void(FaultMsg)>           serFault;
extern dmq::serialization::serializer::Serializer<void(ActuatorStatusMsg)>  serActuator;
extern dmq::serialization::serializer::Serializer<void(SensorStatusMsg)>    serSensor;
extern dmq::serialization::serializer::Serializer<void(HeartbeatMsg)>       serHeartbeat;

void RegisterSerializers();
void RegisterStringifiers();

} // namespace cellutron

#endif
