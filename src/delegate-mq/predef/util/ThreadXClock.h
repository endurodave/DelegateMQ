#ifndef THREADX_CLOCK_H
#define THREADX_CLOCK_H

#include "tx_api.h"
#include <chrono>

namespace dmq {
    struct ThreadXClock {
        // 1. Define duration traits 
        using rep = int64_t;
        using period = std::milli;
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<ThreadXClock>;
        static const bool is_steady = true;

        // 2. The critical "now()" function
        static time_point now() noexcept {
            static ULONG last = 0;
            static uint64_t high = 0;

            // Enter Critical Section
            // This prevents other threads (and ISRs) from interrupting 
            // the read-modify-write of 'last' and 'high'.
            TX_INTERRUPT_SAVE_AREA
            TX_DISABLE

            ULONG cur = tx_time_get();

            constexpr uint64_t TICK_MODULO = static_cast<uint64_t>(1ULL) << (sizeof(ULONG) * 8);

            if (cur < last) {
                high += TICK_MODULO;
            }

            last = cur;

            // Capture value before re-enabling interrupts
            uint64_t ticks = high + cur;

            // Exit Critical Section
            TX_RESTORE

            return time_point(duration(static_cast<rep>(ticks)));
        }
    };
}

#endif // THREADX_CLOCK_H