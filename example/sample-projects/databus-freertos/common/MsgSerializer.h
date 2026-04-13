#ifndef MSG_SERIALIZER_H
#define MSG_SERIALIZER_H

/// @file MsgSerializer.h
/// @brief Serializer type aliases for SensorMsg, CmdMsg, and AlarmMsg.
///        Include in server and client to declare matching serializer instances.
/// @see https://github.com/DelegateMQ/DelegateMQ
/// David Lafreniere, 2026.

#include "Msg.h"

/// Serializer for SensorMsg published on topics::SensorTemp.
using SensorSerializer = Serializer<void(SensorMsg)>;

/// Serializer for CmdMsg published on topics::CmdRate.
using CmdSerializer = Serializer<void(CmdMsg)>;

/// Serializer for AlarmMsg published on topics::AlarmStatus.
/// Shares the same UDP transport as SensorSerializer (port 9000).
using AlarmSerializer = Serializer<void(AlarmMsg)>;

#endif // MSG_SERIALIZER_H
