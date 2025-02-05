#include "DataMgr.h"

DelegateMQ::MulticastDelegateSafe<void(DataMsg&)> DataMgr::DataMsgCb;
