#ifndef HEARTBEAT_MSG_H
#define HEARTBEAT_MSG_H

#include "DelegateMQ.h"

/// @brief Simple heartbeat message.
struct HeartbeatMsg : public serialize::I
{
    uint32_t counter = 0;

    HeartbeatMsg() = default;
    HeartbeatMsg(uint32_t c) : counter(c) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        return ms.read(is, counter, false);
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        return ms.write(os, counter, false);
    }
};

#endif
