// Example.cs — C# client for a C++ DelegateMQ DataBus server over UDP.
//
// Demonstrates:
//   - Receiving DataMsg from the C++ server (sensor/actuator data)
//   - Sending CommandMsg to the C++ server (adjust polling rate)
//
// C++ server build requirements:
//   set(DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
//   set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")   // Windows
//   set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")   // Linux
//
// Start the C++ DataBus server first, then:
//   dotnet run [duration_seconds]

using System;
using System.Threading;
using DelegateMQ.Interop;

// ---------------------------------------------------------------------------
// Configuration — must match C++ server
// ---------------------------------------------------------------------------
const string ServerHost   = "127.0.0.1";
const int DataRecvPort    = 8000;    // C++ server PUBs DataMsg here  — we bind
const int CmdSendPort     = 8001;    // C++ server SUBs CommandMsg here — we send

// Remote IDs — must match C++ SystemIds.h RemoteId enum
const ushort DataMsgId = 100;
const ushort CmdMsgId  = 101;

int duration = args.Length > 0 ? int.Parse(args[0]) : 30;

// ---------------------------------------------------------------------------
// Callback — invoked on the DmqDataBus receive thread
// ---------------------------------------------------------------------------
static void OnDataMsg(ushort remoteId, byte[] payload)
{
    // DataMsg: MSGPACK_DEFINE(actuators, sensors)
    // Decoded as: [ [[id, position, voltage], ...], [[id, supplyV, readingV], ...] ]
    dynamic[] fields    = Serializer.Unpack(payload);
    object[]  actuators = (object[])fields[0];
    object[]  sensors   = (object[])fields[1];

    Console.WriteLine($"[RECV] DataMsg: {actuators.Length} actuator(s), {sensors.Length} sensor(s)");

    foreach (object[] a in actuators)
        Console.WriteLine($"  Actuator  id={a[0]}  pos={a[1]}  voltage={a[2]}V");
    foreach (object[] s in sensors)
        Console.WriteLine($"  Sensor    id={s[0]}  supply={s[1]}V  reading={s[2]}V");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
using var client = new DmqDataBus(ServerHost, DataRecvPort, CmdSendPort);
client.RegisterCallback(DataMsgId, OnDataMsg);
client.Start();

Console.WriteLine($"Connecting to C++ DataBus server at {ServerHost}");
Console.WriteLine($"  Receiving DataMsg  on port {DataRecvPort}");
Console.WriteLine($"  Sending  CommandMsg on port {CmdSendPort}");
Console.WriteLine($"  Running for {duration} seconds...");

// Allow the server time to register our presence before sending
Thread.Sleep(1000);

// Send initial command: request 500 ms polling interval
// CommandMsg: MSGPACK_DEFINE(pollingRateMs)
client.Send(CmdMsgId, Serializer.Pack(500));
Console.WriteLine("[SEND] CommandMsg: pollingRateMs=500");

Thread.Sleep((duration - 1) * 1000);

Console.WriteLine("Done.");
