"""
dmq_databus.py — DataBus participant client for DelegateMQ interop over UDP.

Wire protocol (DmqHeader, 8 bytes, Big Endian / Network Byte Order):

    Offset  Size  Field
    0       2     Marker   (always 0xAA55)
    2       2     ID       (DelegateRemoteId)
    4       2     SeqNum   (sequence number, wraps at 65536)
    6       2     Length   (payload length in bytes)

The header is immediately followed by Length bytes of serialized payload
(MessagePack by convention; see serializer.py).

ACK packet: header only (Length=0), ID=0, SeqNum echoes the received SeqNum.
Sent automatically by the receiver after every non-ACK message.

See ../README.md for the full protocol specification.
"""

import select
import socket
import struct
import threading
from typing import Callable, Dict, Optional

DMQ_MARKER  = 0xAA55
ACK_ID      = 0
HEADER_FMT  = "!HHHH"                        # Big-endian: marker, id, seq, length
HEADER_SIZE = struct.calcsize(HEADER_FMT)    # 8 bytes


class DmqDataBus:
    """
    Generic UDP client for DelegateMQ DataBus.

    Manages two UDP sockets:
      recv_socket — bound to local recv_port, receives incoming messages.
      send_socket — sends outgoing messages and ACKs to remote send_port.

    Registered callbacks are invoked on the background polling thread with
    signature: callback(remote_id: int, payload: bytes).

    Usage:
        client = DmqDataBus("127.0.0.1", recv_port=8000, send_port=8001)
        client.register_callback(100, my_handler)
        client.start()
        client.send(101, payload_bytes)
        ...
        client.stop()

    Alternatively, call process_incoming() in your own loop instead of
    start()/stop() for explicit polling control.
    """

    def __init__(
        self,
        remote_host: str,
        recv_port: int,
        send_port: int,
        recv_timeout: float = 0.1,
    ):
        """
        Args:
            remote_host:   IP address of the C++ DataBus server.
            recv_port:     Local port to bind for incoming messages (C++ PUB port).
            send_port:     Remote port to send messages and ACKs (C++ SUB port).
            recv_timeout:  select() timeout in seconds — controls polling latency.
        """
        self._remote_host  = remote_host
        self._recv_port    = recv_port
        self._send_port    = send_port
        self._recv_timeout = recv_timeout

        self._recv_socket: Optional[socket.socket] = None
        self._send_socket: Optional[socket.socket] = None

        self._seq_num  = 0
        self._seq_lock = threading.Lock()

        self._callbacks: Dict[int, Callable[[int, bytes], None]] = {}

        self._running = False
        self._thread: Optional[threading.Thread] = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self):
        """Open sockets and start the background receive thread."""
        self._recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._recv_socket.bind(("", self._recv_port))

        self._send_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self._running = True
        self._thread  = threading.Thread(target=self._recv_loop, daemon=True,
                                          name="DmqRecv")
        self._thread.start()

    def stop(self):
        """Stop the receive thread and close sockets."""
        self._running = False

        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)

        if self._recv_socket:
            self._recv_socket.close()
            self._recv_socket = None

        if self._send_socket:
            self._send_socket.close()
            self._send_socket = None

    # ------------------------------------------------------------------
    # Registration
    # ------------------------------------------------------------------

    def register_callback(self, remote_id: int,
                          callback: Callable[[int, bytes], None]):
        """
        Register a callback for an incoming remote ID.

        Args:
            remote_id: DelegateRemoteId value declared in C++ SystemIds.h.
            callback:  Called as callback(remote_id, payload_bytes) on the
                       receive thread whenever a matching message arrives.
        """
        self._callbacks[remote_id] = callback

    # ------------------------------------------------------------------
    # Send
    # ------------------------------------------------------------------

    def send(self, remote_id: int, payload: bytes):
        """
        Send a serialized message to the remote DataBus server.

        Args:
            remote_id: DelegateRemoteId matching the C++ subscriber.
            payload:   Serialized bytes (use serializer.pack() for MessagePack).
        """
        if not self._send_socket:
            return

        with self._seq_lock:
            seq = self._seq_num
            self._seq_num = (self._seq_num + 1) % 65536

        packet = struct.pack(HEADER_FMT, DMQ_MARKER, remote_id, seq, len(payload))
        packet += payload

        try:
            self._send_socket.sendto(packet, (self._remote_host, self._send_port))
        except OSError as exc:
            print(f"[DmqDataBus] Send error: {exc}")

    # ------------------------------------------------------------------
    # Manual poll (alternative to start/stop)
    # ------------------------------------------------------------------

    def process_incoming(self) -> bool:
        """
        Poll once for an incoming packet. Non-blocking (uses select with timeout).

        Returns:
            True if a packet was received and dispatched, False otherwise.

        Use this in your own event loop instead of start()/stop() when you
        need explicit control over the polling thread.
        """
        return self._recv_once()

    # ------------------------------------------------------------------
    # Internal receive loop
    # ------------------------------------------------------------------

    def _recv_loop(self):
        while self._running:
            self._recv_once()

    def _recv_once(self) -> bool:
        if not self._recv_socket:
            return False

        ready, _, _ = select.select([self._recv_socket], [], [], self._recv_timeout)
        if not ready:
            return False

        try:
            data, _ = self._recv_socket.recvfrom(65536)
        except OSError:
            return False

        if len(data) < HEADER_SIZE:
            return False

        marker, remote_id, seq, length = struct.unpack(HEADER_FMT, data[:HEADER_SIZE])

        if marker != DMQ_MARKER:
            print(f"[DmqDataBus] Invalid marker: 0x{marker:04X} — packet discarded")
            return False

        # Incoming ACK for a message we sent — no action needed in this client
        if remote_id == ACK_ID:
            return True

        payload = data[HEADER_SIZE:HEADER_SIZE + length]

        # Send ACK back to the server before invoking the callback
        self._send_ack(seq)

        cb = self._callbacks.get(remote_id)
        if cb:
            try:
                cb(remote_id, payload)
            except Exception as exc:
                print(f"[DmqDataBus] Callback error for id={remote_id}: {exc}")

        return True

    def _send_ack(self, seq: int):
        if not self._send_socket:
            return
        ack = struct.pack(HEADER_FMT, DMQ_MARKER, ACK_ID, seq, 0)
        try:
            self._send_socket.sendto(ack, (self._remote_host, self._send_port))
        except OSError:
            pass
