#ifndef CENTRIFUGE_STATUS_MSG_H
#define CENTRIFUGE_STATUS_MSG_H

#include "DelegateMQ.h"
#include <cstdint>

/// @brief Message containing the current centrifuge status.
/// @details IO CPU broadcasts this every 10ms.
struct CentrifugeStatusMsg : public serialize::I
{
    uint16_t rpm = 0;

    CentrifugeStatusMsg() = default;
    CentrifugeStatusMsg(uint16_t val) : rpm(val) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        return ms.read(is, rpm, false);
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        return ms.write(os, rpm, false);
    }
};

#endif
