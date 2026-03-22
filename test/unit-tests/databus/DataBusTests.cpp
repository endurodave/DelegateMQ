#include "DelegateMQ.h"
#include <iostream>

#if defined(DMQ_DATABUS)

extern int DataBusLocalTestMain();
extern int DataBusQosTestMain();
extern int DataBusRemoteTestMain();
extern int DataBusSpyTestMain();
extern int DataBusBenchmarkTestMain();
extern int DataBusTypeMismatchTestMain();

void RunDataBusTests() {
    std::cout << "--- Running DataBus Unit Tests ---" << std::endl;
    DataBusLocalTestMain();
    DataBusQosTestMain();
    DataBusRemoteTestMain();
    DataBusSpyTestMain();
    DataBusBenchmarkTestMain();
    DataBusTypeMismatchTestMain();
    std::cout << "--- DataBus Unit Tests Completed ---" << std::endl;
}

#endif
