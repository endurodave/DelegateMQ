// DmqDataBus.cs — DataBus participant client for DelegateMQ interop over UDP.
//
// Wire protocol (DmqHeader, 8 bytes, Network Byte Order / Big Endian):
//
//   Offset  Size  Field
//   0       2     Marker   (always 0xAA55)
//   2       2     ID       (DelegateRemoteId)
//   4       2     SeqNum   (sequence number, wraps at 65536)
//   6       2     Length   (payload length in bytes)
//
// The header is immediately followed by Length bytes of serialized payload
// (MessagePack by convention; see Serializer.cs).
//
// ACK packet: header only (Length=0), ID=0, SeqNum echoes the received SeqNum.
// Sent automatically after every non-ACK message received.
//
// See ../README.md for the full protocol specification.

using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace DelegateMQ.Interop
{
    /// <summary>
    /// Generic UDP client for DelegateMQ DataBus.
    ///
    /// Manages two UDP sockets:
    ///   recv socket — bound to local recvPort, receives incoming messages.
    ///   send socket — sends outgoing messages and ACKs to remoteHost:sendPort.
    ///
    /// Registered callbacks are invoked on the background receive thread with
    /// signature: Action&lt;ushort remoteId, byte[] payload&gt;.
    /// </summary>
    public sealed class DmqDataBus : IDisposable
    {
        private const ushort DmqMarker = 0xAA55;
        private const ushort AckId     = 0;
        private const int    HeaderSize = 8;

        private readonly string _remoteHost;
        private readonly int    _recvPort;
        private readonly int    _sendPort;

        private UdpClient?  _recvClient;
        private UdpClient?  _sendClient;
        private IPEndPoint? _sendEndPoint;

        private ushort         _seqNum;
        private readonly object _seqLock = new();

        private readonly Dictionary<ushort, Action<ushort, byte[]>> _callbacks = new();

        private Thread?         _recvThread;
        private volatile bool   _running;

        /// <summary>
        /// Create a new DmqDataBus.
        /// </summary>
        /// <param name="remoteHost">IP address of the C++ DataBus server.</param>
        /// <param name="recvPort">Local port to bind for incoming messages (C++ PUB port).</param>
        /// <param name="sendPort">Remote port to send messages and ACKs (C++ SUB port).</param>
        public DmqDataBus(string remoteHost, int recvPort, int sendPort)
        {
            _remoteHost = remoteHost;
            _recvPort   = recvPort;
            _sendPort   = sendPort;
        }

        // ------------------------------------------------------------------
        // Lifecycle
        // ------------------------------------------------------------------

        /// <summary>Open sockets and start the background receive thread.</summary>
        public void Start()
        {
            _recvClient   = new UdpClient(_recvPort);
            _sendClient   = new UdpClient();
            _sendEndPoint = new IPEndPoint(IPAddress.Parse(_remoteHost), _sendPort);

            _running    = true;
            _recvThread = new Thread(RecvLoop)
            {
                IsBackground = true,
                Name         = "DmqRecv",
            };
            _recvThread.Start();
        }

        /// <summary>Stop the receive thread and close sockets.</summary>
        public void Stop()
        {
            _running = false;
            _recvClient?.Close();   // Unblocks UdpClient.Receive()
            _sendClient?.Close();
            _recvThread?.Join(2000);
            _recvClient = null;
            _sendClient = null;
        }

        /// <inheritdoc/>
        public void Dispose() => Stop();

        // ------------------------------------------------------------------
        // Registration
        // ------------------------------------------------------------------

        /// <summary>
        /// Register a callback for an incoming remote ID.
        /// </summary>
        /// <param name="remoteId">DelegateRemoteId value declared in C++ SystemIds.h.</param>
        /// <param name="callback">
        ///   Invoked as callback(remoteId, payloadBytes) on the receive thread
        ///   whenever a matching message arrives.
        /// </param>
        public void RegisterCallback(ushort remoteId, Action<ushort, byte[]> callback)
        {
            _callbacks[remoteId] = callback;
        }

        // ------------------------------------------------------------------
        // Send
        // ------------------------------------------------------------------

        /// <summary>
        /// Send a serialized message to the remote DataBus server.
        /// </summary>
        /// <param name="remoteId">DelegateRemoteId matching the C++ subscriber.</param>
        /// <param name="payload">MessagePack-serialized bytes (use Serializer.Pack()).</param>
        public void Send(ushort remoteId, byte[] payload)
        {
            if (_sendClient == null)
                return;

            ushort seq;
            lock (_seqLock)
                seq = _seqNum++;

            byte[] packet = BuildPacket(remoteId, seq, payload);
            try
            {
                _sendClient.Send(packet, packet.Length, _sendEndPoint);
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[DmqDataBus] Send error: {ex.Message}");
            }
        }

        // ------------------------------------------------------------------
        // Internal receive loop
        // ------------------------------------------------------------------

        private void RecvLoop()
        {
            var ep = new IPEndPoint(IPAddress.Any, _recvPort);
            while (_running)
            {
                try
                {
                    byte[] data = _recvClient!.Receive(ref ep);
                    ProcessPacket(data);
                }
                catch (SocketException)
                {
                    // Socket closed during Stop() — exit cleanly
                    break;
                }
                catch (Exception ex)
                {
                    if (_running)
                        Console.Error.WriteLine($"[DmqDataBus] Recv error: {ex.Message}");
                }
            }
        }

        private void ProcessPacket(byte[] data)
        {
            if (data.Length < HeaderSize)
                return;

            ushort marker   = ReadUInt16BE(data, 0);
            ushort remoteId = ReadUInt16BE(data, 2);
            ushort seq      = ReadUInt16BE(data, 4);
            ushort length   = ReadUInt16BE(data, 6);

            if (marker != DmqMarker)
            {
                Console.Error.WriteLine($"[DmqDataBus] Invalid marker: 0x{marker:X4} — packet discarded");
                return;
            }

            // Incoming ACK for a message we sent — no action needed in this client
            if (remoteId == AckId)
                return;

            byte[] payload = new byte[length];
            if (length > 0 && data.Length >= HeaderSize + length)
                Array.Copy(data, HeaderSize, payload, 0, length);

            // Send ACK back before invoking the callback
            SendAck(seq);

            if (_callbacks.TryGetValue(remoteId, out var cb))
            {
                try
                {
                    cb(remoteId, payload);
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine(
                        $"[DmqDataBus] Callback error for id={remoteId}: {ex.Message}");
                }
            }
        }

        private void SendAck(ushort seq)
        {
            if (_sendClient == null)
                return;
            byte[] ack = BuildPacket(AckId, seq, Array.Empty<byte>());
            try { _sendClient.Send(ack, ack.Length, _sendEndPoint); }
            catch { /* ignore */ }
        }

        // ------------------------------------------------------------------
        // Packet builder and Big-Endian helpers (Network Byte Order)
        // ------------------------------------------------------------------

        private static byte[] BuildPacket(ushort id, ushort seq, byte[] payload)
        {
            byte[] packet = new byte[HeaderSize + payload.Length];
            WriteUInt16BE(packet, 0, DmqMarker);
            WriteUInt16BE(packet, 2, id);
            WriteUInt16BE(packet, 4, seq);
            WriteUInt16BE(packet, 6, (ushort)payload.Length);
            if (payload.Length > 0)
                Array.Copy(payload, 0, packet, HeaderSize, payload.Length);
            return packet;
        }

        private static ushort ReadUInt16BE(byte[] buf, int offset) =>
            (ushort)((buf[offset] << 8) | buf[offset + 1]);

        private static void WriteUInt16BE(byte[] buf, int offset, ushort value)
        {
            buf[offset]     = (byte)(value >> 8);
            buf[offset + 1] = (byte)(value & 0xFF);
        }
    }
}
