#include "DataMgr.h"

// Initialize the static signal
std::shared_ptr<dmq::SignalSafe<void(DataMsg&)>> DataMgr::DataMsgCb =
    std::make_shared<dmq::SignalSafe<void(DataMsg&)>>();