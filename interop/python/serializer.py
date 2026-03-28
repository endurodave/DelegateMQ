"""
serializer.py — MessagePack helpers for DelegateMQ DataBus interop.

C++ DataBus applications using DMQ_SERIALIZE_MSGPACK serialize each message
as a flat MessagePack array whose elements match the MSGPACK_DEFINE field order:

    struct CommandMsg {
        int pollingRateMs = 1000;
        MSGPACK_DEFINE(pollingRateMs);       // wire: [1000]
    };

    struct DataMsg {
        std::vector<Actuator> actuators;
        std::vector<Sensor>   sensors;
        MSGPACK_DEFINE(actuators, sensors);  // wire: [[[id,pos,v],...], [[id,sup,read],...]]
    };

Use pack() to encode outgoing messages and unpack() to decode incoming ones.

Requires: pip install msgpack
"""

from typing import Any, List
import msgpack


def pack(*args) -> bytes:
    """
    Encode one or more values as a MessagePack array.

    Matches C++ MSGPACK_DEFINE(field1, field2, ...) serialization.
    Pass each top-level field as a separate argument.

    Examples:
        pack(500)                    # CommandMsg(pollingRateMs=500)
        pack([[1, True, 3.3]],       # DataMsg with one actuator, no sensors
             [])
    """
    return msgpack.packb(list(args), use_bin_type=True)


def unpack(data: bytes) -> List[Any]:
    """
    Decode a MessagePack payload into a Python list.

    The list elements are in the same order as the C++ MSGPACK_DEFINE fields.
    Nested objects (vectors of structs) are returned as nested lists.

    Example:
        fields = unpack(payload)
        actuators = fields[0]    # list of [id, position, voltage]
        sensors   = fields[1]    # list of [id, supplyV, readingV]
    """
    return msgpack.unpackb(data, raw=False)
