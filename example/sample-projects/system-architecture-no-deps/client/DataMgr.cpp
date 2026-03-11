#include "DataMgr.h"

// Initialize the static signal
dmq::SignalPtr<void(DataMsg&)> DataMgr::DataMsgCb =
    dmq::MakeSignal<void(DataMsg&)>();