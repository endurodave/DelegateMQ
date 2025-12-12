#ifndef ITRANSPORT_MONITOR_H
#define ITRANSPORT_MONITOR_H

//#include "DmqHeader.h"
//#include <sstream>

/// @TODO Implement the ITransportMonitor interface if necessary.
/// @brief DelegateMQ transport monitor interface. 
class ITransportMonitor
{
public:
    /// Add a sequence number
    /// param[in] seqNum - the message sequence number
    /// param[in] remoteId - the remote ID
    virtual void Add(uint16_t seqNum, dmq::DelegateRemoteId remoteId) = 0;

    /// Remove a sequence number
    /// param[in] seqNum - the message sequence number
    virtual void Remove(uint16_t seqNum) = 0;
};

#endif