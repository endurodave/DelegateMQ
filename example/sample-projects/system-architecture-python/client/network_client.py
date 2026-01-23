# Python ZeroMQ client application communicates with C++ NetworkMgr.

import zmq
import msgpack
import struct
import threading
from typing import Callable, Dict

# Import configuration
from config import SERVER_IP, SEND_PORT, RECV_PORT, DMQ_MARKER, RemoteId

# Import message definitions for deserialization
from messages import AlarmMsg, CommandMsg, DataMsg, ActuatorMsg, AlarmNote

class NetworkClient:
    def __init__(self):
        self._context = zmq.Context()
        self._send_socket = None
        self._recv_socket = None
        
        self._running = False
        self._recv_thread = None
        
        # Sequence number for outgoing messages
        self._seq_num = 0
        self._seq_lock = threading.Lock()

        # Callbacks map: ID -> Function
        self._callbacks: Dict[int, Callable] = {}

    def start(self):
        """Connects sockets and starts the receive thread."""
        print(f"Connecting to C++ Server at {SERVER_IP}...")

        # 1. Setup Send Socket (PAIR) - Connects to C++ Receiver
        self._send_socket = self._context.socket(zmq.PAIR)
        self._send_socket.connect(f"tcp://{SERVER_IP}:{SEND_PORT}")

        # 2. Setup Recv Socket (PAIR) - Connects to C++ Sender
        self._recv_socket = self._context.socket(zmq.PAIR)
        self._recv_socket.connect(f"tcp://{SERVER_IP}:{RECV_PORT}")
        
        # 3. Start Listener Thread
        self._running = True
        self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._recv_thread.start()
        print("NetworkClient started.")

    def stop(self):
        """Stops the client."""
        self._running = False
        if self._recv_thread:
            self._recv_thread.join()
        
        if self._send_socket: self._send_socket.close()
        if self._recv_socket: self._recv_socket.close()
        self._context.term()

    def register_callback(self, remote_id: int, func: Callable):
        """Register a python function to handle a specific C++ message ID."""
        self._callbacks[remote_id] = func

    def send(self, remote_id: int, msg_obj):
        """
        Serializes and sends a message to C++.
        Format: [Marker(2)][ID(2)][Seq(2)][MsgPack Payload]
        """
        if not self._send_socket:
            print("Error: Socket not connected.")
            return

        # 1. Prepare Header
        with self._seq_lock:
            self._seq_num = (self._seq_num + 1) % 65535
            seq = self._seq_num

        # 2. Prepare Payload (MsgPack)
        payload = msg_obj.to_msgpack()

        length = len(payload)

        # struct.pack('<HHHH'): Little-endian. We rely on DMQ_MARKER being pre-swapped.
        header = struct.pack('<HHHH', DMQ_MARKER, remote_id, seq, length)

        # 3. Send Packet
        full_packet = header + payload
        
        try:
            self._send_socket.send(full_packet)
        except zmq.ZMQError as e:
            print(f"Send failed: {e}")

    def _send_ack(self, seq_num):
        """Sends an ACK back to C++ so it stops waiting (if blocking)."""
        # Header with ID = 0 (ACK) and the RECEIVED sequence number
        header = struct.pack('<HHHH', DMQ_MARKER, RemoteId.ACK, seq_num, 0)
        try:
            self._send_socket.send(header) # ACK has no payload
        except zmq.ZMQError:
            pass

    def _recv_loop(self):
        """Background thread to handle incoming messages."""
        poller = zmq.Poller()
        poller.register(self._recv_socket, zmq.POLLIN)

        while self._running:
            try:
                # Poll with 100ms timeout
                socks = dict(poller.poll(100))
                if self._recv_socket in socks and socks[self._recv_socket] == zmq.POLLIN:
                    
                    # 1. Receive Raw Bytes
                    data = self._recv_socket.recv()
                    
                    if len(data) < 8:
                        continue # Malformed

                    # 2. Parse Header (First 8 bytes)
                    marker, remote_id, seq_num, length = struct.unpack('<HHHH', data[:8])

                    if marker != DMQ_MARKER:
                        print(f"Warning: Invalid Marker {hex(marker)}")
                        continue

                    # 3. Handle ACK vs Data
                    if remote_id == RemoteId.ACK:
                        # C++ acknowledged our message. 
                        pass
                    else:
                        # 4. Extract Payload
                        payload = data[8:]
                        self._dispatch_message(remote_id, payload)
                        
                        # 5. Send ACK back to C++
                        self._send_ack(seq_num)

            except Exception as e:
                print(f"Receive Error: {e}")
                break

    def _dispatch_message(self, remote_id, payload):
        """Deserializes payload based on ID and calls registered callback."""
        try:
            if remote_id not in self._callbacks:
                print(f"Warning: No callback for ID {remote_id}")
                return

            callback = self._callbacks[remote_id]

            unpacker = msgpack.Unpacker()
            unpacker.feed(payload)
            
            args = []
            for unpacked_item in unpacker:
                args.append(unpacked_item)

            if remote_id == RemoteId.ALARM:
                # AlarmMsgCb -> void(AlarmMsg&, AlarmNote&)
                msg = AlarmMsg.from_msgpack(msgpack.packb(args[0]))
                note = AlarmNote.from_msgpack(msgpack.packb(args[1]))
                callback(msg, note)

            elif remote_id == RemoteId.COMMAND:
                # CommandMsgCb -> void(CommandMsg&)
                msg = CommandMsg.from_msgpack(msgpack.packb(args[0]))
                callback(msg)

            elif remote_id == RemoteId.DATA:
                # DataMsgCb -> void(DataMsg&)
                msg = DataMsg.from_msgpack(msgpack.packb(args[0]))
                callback(msg)
                
            elif remote_id == RemoteId.ACTUATOR:
                # ActuatorMsgCb -> void(ActuatorMsg&)
                # Placeholder for Actuator logic
                print("Received Actuator Msg")
                
        except Exception as e:
            print(f"Dispatch Error for ID {remote_id}: {e}")