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
