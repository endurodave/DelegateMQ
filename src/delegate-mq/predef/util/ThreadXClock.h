#ifndef THREADX_CLOCK_H
#define THREADX_CLOCK_H

#include "tx_api.h"
#include <chrono>

namespace dmq {
    struct ThreadXClock {
        // 1. Define duration traits 
        // ASSUMPTION: ThreadX is configured for 1000Hz (1ms) ticks.
        // If your TX_TIMER_TICKS_PER_SECOND is 100, change std::milli to std::ratio<1, 100>
        using rep = int64_t;
        using period = std::milli; 
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<ThreadXClock>;
        static const bool is_steady = true;

        // 2. The critical "now()" function
        static time_point now() noexcept {
            // tx_time_get() returns ULONG (32-bit). 
            // We cast to int64_t for chrono compatibility.
            // Note: 32-bit ticks at 1ms wrap every ~49 days.
            return time_point(duration(tx_time_get()));
        }
    };
}

#endif // THREADX_CLOCK_H