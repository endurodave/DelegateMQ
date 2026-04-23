#ifndef RUN_STATUS_MSG_H
#define RUN_STATUS_MSG_H

#include "DelegateMQ.h"
#include <cstdint>
#include <string>

enum class RunStatus { IDLE, PROCESSING, ABORTING, FAULT };

/// @brief Message containing the current run status.
struct RunStatusMsg : public serialize::I
{
    RunStatus status = RunStatus::IDLE;

    RunStatusMsg() = default;
    RunStatusMsg(RunStatus s) : status(s) {}

    virtual std::istream& read(serialize& ms, std::istream& is) override {
        uint8_t s;
        ms.read(is, s);
        status = static_cast<RunStatus>(s);
        return is;
    }

    virtual std::ostream& write(serialize& ms, std::ostream& os) override {
        uint8_t s = static_cast<uint8_t>(status);
        ms.write(os, s);
        return os;
    }
};

#endif
