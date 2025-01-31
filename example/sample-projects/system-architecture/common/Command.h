#ifndef COMMAND_H
#define COMMAND_H

#include <msgpack.hpp>
#include <chrono>

class Command
{
public:
    enum class Action { START, STOP };

    Action action = Action::START;
    uint32_t pollTime = 0;

    MSGPACK_DEFINE(action, pollTime);
};

// Specialize the msgpack serialization for Command::Action
MSGPACK_ADD_ENUM(Command::Action);


#endif