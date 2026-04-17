#ifndef FAULT_MSG_H
#define FAULT_MSG_H

#include "DelegateMQ.h"
#include <string>

/// @brief Message to notify system of a critical fault.
struct FaultMsg : public serialize::I
{
    uint8_t faultCode = 0;

    FaultMsg() = default;
    FaultMsg(uint8_t code) : faultCode(code) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        return ms.read(is, faultCode, false);
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        return ms.write(os, faultCode, false);
    }
};

#endif
