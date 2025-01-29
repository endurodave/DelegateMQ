#ifndef DATA_PACKAGE_H
#define DATA_PACKAGE_H

#include "Actuator.h"
#include "Sensor.h"
#include <msgpack.hpp>
#include <vector>

class DataPackage
{
public:
    std::vector<ActuatorState> actuators;
    std::vector<SensorData> sensors;

    MSGPACK_DEFINE(actuators, sensors);
};

#endif