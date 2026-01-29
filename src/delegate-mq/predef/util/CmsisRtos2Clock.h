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
            // osKernelGetTickCount returns uint32_t
            return time_point(duration(osKernelGetTickCount()));
        }
    };
}

#endif // CMSIS_RTOS2_CLOCK_H