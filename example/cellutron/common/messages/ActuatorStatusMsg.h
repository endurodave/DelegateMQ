#ifndef ACTUATOR_STATUS_MSG_H
#define ACTUATOR_STATUS_MSG_H

#include "DelegateMQ.h"
#include <string>

enum class ActuatorType { VALVE, PUMP };

struct ActuatorStatusMsg : public serialize::I
{
    ActuatorType type = ActuatorType::VALVE;
    uint8_t id = 0;
    int16_t value = 0; // 0/1 for valve, -100 to 100 for pump

    ActuatorStatusMsg() = default;
    ActuatorStatusMsg(ActuatorType t, uint8_t i, int16_t v) : type(t), id(i), value(v) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        uint8_t t;
        ms.read(is, t, false);
        type = static_cast<ActuatorType>(t);
        ms.read(is, id, false);
        ms.read(is, value, false);
        return is;
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        uint8_t t = static_cast<uint8_t>(type);
        ms.write(os, t, false);
        ms.write(os, id, false);
        ms.write(os, value, false);
        return os;
    }
};

#endif
