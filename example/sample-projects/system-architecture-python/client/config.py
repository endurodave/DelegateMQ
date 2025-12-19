from enum import IntEnum

# ------------------------------------------------------------------------------
# Network Configuration
# ------------------------------------------------------------------------------
SERVER_IP = "localhost"
SEND_PORT = 5556  # Port the C++ Server Listens on (We send here)
RECV_PORT = 5555  # Port the C++ Server Publishes on (We receive here)

# Sync Marker
# The C++ server expects the marker 0xAA55. 
# Because we use Little Endian packing ('<HHH'), we must swap the bytes here
# (0x55AA) so they appear as [0xAA, 0x55] on the wire.
DMQ_MARKER = 0x55AA

# ------------------------------------------------------------------------------
# Protocol IDs
# ------------------------------------------------------------------------------
# These IDs must match the C++ "RemoteIds.h" file exactly.
class RemoteId(IntEnum):
    ACK         = 0  # Internal DelegateMQ ACK ID
    ALARM       = 1
    DATA        = 2
    COMMAND     = 3
    ACTUATOR    = 4