#ifndef SYSTEM_IDS_H
#define SYSTEM_IDS_H

#include "DelegateMQ.h"

namespace SystemTopic {
    // Topic names
    static const char* const DataMsg = "Topic/DataMsg";

    // Remote IDs (DelegateRemoteId) for network distribution
    // These must match between client and server
    enum RemoteId : dmq::DelegateRemoteId {
        DataMsgId = 100
    };
}

#endif // SYSTEM_IDS_H
