#ifndef CMSIS_RTOS2_CLOCK_H
#define CMSIS_RTOS2_CLOCK_H

#include "cmsis_os2.h"
#include <chrono>

namespace dmq {
    struct CmsisRtos2Clock {
        // Assume 1 tick = 1 millisecond. 
        // If your RTOS tick is different, change std::milli to your ratio.
        using rep = int64_t;
        using period = std::milli;
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<CmsisRtos2Clock>;
        static const bool is_steady = true;

        static time_point now() noexcept {
            // Static state to track the 32-bit rollover.
            // osKernelGetTickCount() wraps every ~49.7 days (at 1ms tick).

            // NOTE: This implementation relies on static state.
            // 1. It must be called at least once every 49 days to detect the wrap.
            // 2. Thread safety: We use osKernelLock to prevent context switches,
            //    ensuring atomic access to 'last' and 'high' across threads.
            static uint32_t last = 0;
            static uint64_t high = 0;

            // Lock the scheduler. 
            // This prevents other threads from interrupting the read-modify-write.
            // Note: This does NOT block ISRs. If you call now() from an ISR, 
            // you may still have race conditions. This is designed for Task-level usage.
            int32_t lockState = osKernelLock();

            uint32_t cur = osKernelGetTickCount();

            // Check for wrap-around (current time is less than last seen time)
            if (cur < last) {
                // Add 2^32 to the high part accumulator
                high += 0x100000000ULL;
            }

            last = cur;

            // Combine the high part with the current low part.
            uint64_t ticks = high + cur;

            // Restore the scheduler lock state
            osKernelRestoreLock(lockState);

            return time_point(duration(static_cast<rep>(ticks)));
        }
    };
}

#endif // CMSIS_RTOS2_CLOCK_H