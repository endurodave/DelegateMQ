#include "DataMgr.h"

dmq::MulticastDelegateSafe<void(DataMsg&)> DataMgr::DataMsgCb;
