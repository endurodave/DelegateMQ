// Serializer.cs — MessagePack helpers for DelegateMQ DataBus interop.
//
// C++ DataBus applications using DMQ_SERIALIZE_MSGPACK serialize each message
// as a flat MessagePack array whose elements match the MSGPACK_DEFINE field order:
//
//   struct CommandMsg {
//       int pollingRateMs = 1000;
//       MSGPACK_DEFINE(pollingRateMs);       // wire: [1000]
//   };
//
//   struct DataMsg {
//       std::vector<Actuator> actuators;
//       std::vector<Sensor>   sensors;
//       MSGPACK_DEFINE(actuators, sensors);  // wire: [[[id,pos,v],...],[[id,sup,read],...]]
//   };
//
// Use Pack() to encode outgoing messages and Unpack() to decode incoming ones.
//
// Requires NuGet package: MessagePack
//   dotnet add package MessagePack

using System.Collections.Generic;
using MessagePack;

namespace DelegateMQ.Interop
{
    /// <summary>
    /// MessagePack encode/decode helpers for DelegateMQ DataBus interop.
    /// </summary>
    public static class Serializer
    {
        /// <summary>
        /// Encode a single value as a 1-element MessagePack array.
        /// Matches: MSGPACK_DEFINE(field1)
        /// </summary>
        public static byte[] Pack<T1>(T1 v1) =>
            MessagePackSerializer.Serialize(new List<object?> { v1 });

        /// <summary>
        /// Encode two values as a 2-element MessagePack array.
        /// Matches: MSGPACK_DEFINE(field1, field2)
        /// </summary>
        public static byte[] Pack<T1, T2>(T1 v1, T2 v2) =>
            MessagePackSerializer.Serialize(new List<object?> { v1, v2 });

        /// <summary>
        /// Encode three values as a 3-element MessagePack array.
        /// Matches: MSGPACK_DEFINE(field1, field2, field3)
        /// </summary>
        public static byte[] Pack<T1, T2, T3>(T1 v1, T2 v2, T3 v3) =>
            MessagePackSerializer.Serialize(new List<object?> { v1, v2, v3 });

        /// <summary>
        /// Decode a MessagePack payload into a dynamic array.
        /// Elements are in the same order as the C++ MSGPACK_DEFINE fields.
        ///
        /// Example:
        ///   var fields    = Serializer.Unpack(payload);
        ///   var actuators = (object[])fields[0];   // list of [id, pos, v]
        ///   var sensors   = (object[])fields[1];   // list of [id, supV, readV]
        /// </summary>
        public static dynamic[] Unpack(byte[] data) =>
            MessagePackSerializer.Deserialize<dynamic[]>(data);
    }
}
