#ifndef STOP_PROCESS_MSG_H
#define STOP_PROCESS_MSG_H

#include "DelegateMQ.h"

/// @brief Message to stop/abort the cell process.
struct StopProcessMsg : public serialize::I
{
    uint8_t dummy = 0xBB;

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        return ms.read(is, dummy, false);
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        return ms.write(os, dummy, false);
    }
};

#endif
