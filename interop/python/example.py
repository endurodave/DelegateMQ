"""
example.py — Python client for a C++ DelegateMQ DataBus server over UDP.

Demonstrates:
  - Receiving DataMsg from the C++ server (sensor/actuator data)
  - Sending CommandMsg to the C++ server (adjust polling rate)

C++ server build requirements:
    set(DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")   # Windows
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")   # Linux

Start the C++ DataBus server first, then run:
    python example.py [duration_seconds]
"""

import sys
import time

from dmq_databus import DmqDataBus
from serializer import pack, unpack

# ---------------------------------------------------------------------------
# Configuration — must match C++ server
# ---------------------------------------------------------------------------
SERVER_HOST    = "127.0.0.1"
DATA_RECV_PORT = 8000    # C++ server PUBs DataMsg here  — we bind and receive
CMD_SEND_PORT  = 8001    # C++ server SUBs CommandMsg here — we send

# Remote IDs — must match C++ SystemIds.h RemoteId enum
DATA_MSG_ID = 100
CMD_MSG_ID  = 101

DURATION = int(sys.argv[1]) if len(sys.argv) > 1 else 30

# ---------------------------------------------------------------------------
# Message decode/encode helpers
# ---------------------------------------------------------------------------

def decode_data_msg(payload: bytes):
    """
    Decode DataMsg payload.
    C++: MSGPACK_DEFINE(actuators, sensors)
    Wire: [ [[id, position, voltage], ...], [[id, supplyV, readingV], ...] ]
    """
    fields    = unpack(payload)
    actuators = fields[0]   # list of [id, position, voltage]
    sensors   = fields[1]   # list of [id, supplyV, readingV]
    return actuators, sensors

def encode_command_msg(polling_rate_ms: int) -> bytes:
    """
    Encode CommandMsg payload.
    C++: MSGPACK_DEFINE(pollingRateMs)
    Wire: [pollingRateMs]
    """
    return pack(polling_rate_ms)

# ---------------------------------------------------------------------------
# Callbacks — invoked on the DmqDataBus receive thread
# ---------------------------------------------------------------------------

def on_data_msg(remote_id: int, payload: bytes):
    actuators, sensors = decode_data_msg(payload)
    print(f"[RECV] DataMsg: {len(actuators)} actuator(s), {len(sensors)} sensor(s)")
    for a in actuators:
        print(f"  Actuator  id={a[0]}  pos={a[1]:.2f}  voltage={a[2]:.2f}V")
    for s in sensors:
        print(f"  Sensor    id={s[0]}  supply={s[1]:.2f}V  reading={s[2]:.2f}V")

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    client = DmqDataBus(SERVER_HOST, DATA_RECV_PORT, CMD_SEND_PORT)
    client.register_callback(DATA_MSG_ID, on_data_msg)
    client.start()

    print(f"Connecting to C++ DataBus server at {SERVER_HOST}")
    print(f"  Receiving DataMsg  on port {DATA_RECV_PORT}")
    print(f"  Sending  CommandMsg on port {CMD_SEND_PORT}")
    print(f"  Running for {DURATION} seconds...")

    # Allow the server time to register our presence before sending
    time.sleep(1)

    # Send initial command: request 500 ms polling interval
    client.send(CMD_MSG_ID, encode_command_msg(500))
    print("[SEND] CommandMsg: pollingRateMs=500")

    # Run for DURATION seconds then exit cleanly
    time.sleep(DURATION - 1)

    client.stop()
    print("Done.")
