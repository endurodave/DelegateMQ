# DelegateMQ Python Interop

Python client for communicating with a C++ DelegateMQ DataBus server over UDP.

## Files

| File            | Purpose                                               |
|-----------------|-------------------------------------------------------|
| `dmq_databus.py` | DataBus participant — UDP framing, ACKs, polling      |
| `serializer.py` | MessagePack encode/decode helpers                     |
| `example.py`    | Runnable example pairing with a C++ DataBus server    |

## Requirements

```
pip install msgpack
```

## Quick Start

```python
from dmq_client import DmqDataBus
from serializer import pack, unpack

def on_data(remote_id, payload):
    fields = unpack(payload)          # fields[0] = actuators, fields[1] = sensors
    print(f"Received DataMsg: {fields}")

client = DmqDataBus("127.0.0.1", recv_port=8000, send_port=8001)
client.register_callback(100, on_data)   # 100 = DataMsgId
client.start()

# Send a CommandMsg (pollingRateMs=500)
client.send(101, pack(500))              # 101 = CommandMsgId

import time; time.sleep(30)
client.stop()
```

## Running the Example

Start the C++ DataBus server first (built with `DMQ_SERIALIZE_MSGPACK`), then:

```
python example.py [duration_seconds]
```

## API

### `DmqDataBus(remote_host, recv_port, send_port, recv_timeout=0.1)`

- `remote_host` — IP address of the C++ server
- `recv_port` — local port to bind for incoming messages (C++ PUB port)
- `send_port` — remote port to send to (C++ SUB port)
- `recv_timeout` — select timeout in seconds (controls polling latency)

### `client.register_callback(remote_id, callback)`

Register a callback for an incoming remote ID.
Signature: `callback(remote_id: int, payload: bytes)`
The callback is invoked on the background polling thread.

### `client.send(remote_id, payload)`

Send a MessagePack-serialized message. Use `serializer.pack()` to encode.

### `client.start()` / `client.stop()`

Open sockets and start the background receive thread / stop and close.

### `client.process_incoming()` → `bool`

Manual single-poll alternative to `start()`/`stop()`. Returns `True` if a
message was received. Use this in your own loop if you need explicit control.

## Thread Safety

`register_callback()` and `send()` are safe to call from any thread.
Callbacks are invoked on the internal receive thread — keep them short or
dispatch to your own thread if needed.
