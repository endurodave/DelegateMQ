#include "DelegateMQ.h"
#include <iostream>
#include <string>

#if defined(DMQ_DATABUS)

// This test intentionally triggers a runtime type mismatch, which calls FaultHandler.
// Since FaultHandler calls abort(), this test is for manual verification and 
// is disabled by default.
//
// HOW TO VERIFY MANUALLY:
// 1. Set #if 1 below.
// 2. Build and run.
// 3. Confirm output shows "FaultHandler called" and application terminates with non-zero exit code.
int DataBusTypeMismatchTestMain() {
#if 0
    std::cout << "Starting DataBusTypeMismatchTest (EXPECTED TO ABORT)..." << std::endl;
    dmq::DataBus::ResetForTesting();

    // 1. Publish as int
    dmq::DataBus::Publish<int>("type/topic", 1);

    // 2. Attempt to subscribe as float on the SAME topic string.
    // This triggers the internal std::type_index check in GetOrCreateSignal.
    std::cout << "Attempting mismatched Subscribe<float> on topic 'type/topic' (initially int)..." << std::endl;
    auto conn = dmq::DataBus::Subscribe<float>("type/topic", [](float val) {
        (void)val;
    });

    std::cerr << "ERROR: If you see this, the type mismatch check FAILED." << std::endl;
#endif
    return 0;
}

#else

int DataBusTypeMismatchTestMain() {
    return 0;
}

#endif
