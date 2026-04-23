#ifndef SENSOR_STATUS_MSG_H
#define SENSOR_STATUS_MSG_H

#include "DelegateMQ.h"

enum class SensorType { PRESSURE, AIR_IN_LINE };

struct SensorStatusMsg : public serialize::I
{
    SensorType type = SensorType::PRESSURE;
    int16_t value = 0;

    SensorStatusMsg() = default;
    SensorStatusMsg(SensorType t, int16_t v) : type(t), value(v) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        uint8_t t;
        ms.read(is, t);
        type = static_cast<SensorType>(t);
        ms.read(is, value);
        return is;
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        uint8_t t = static_cast<uint8_t>(type);
        ms.write(os, t);
        ms.write(os, value);
        return os;
    }
};

#endif
