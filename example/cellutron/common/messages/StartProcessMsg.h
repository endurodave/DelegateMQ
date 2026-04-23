#ifndef START_PROCESS_MSG_H
#define START_PROCESS_MSG_H

#include "DelegateMQ.h"

/// @brief Message to start the cell process.
struct StartProcessMsg : public serialize::I
{
    // Adding a dummy field to ensure msg_serialize has something to write
    uint8_t dummy = 0xAA;

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        return ms.read(is, dummy);
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        return ms.write(os, dummy);
    }
};

#endif
