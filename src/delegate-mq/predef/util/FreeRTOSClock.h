#ifndef FREERTOS_CLOCK_H
#define FREERTOS_CLOCK_H

#include "FreeRTOS.h"
#include "task.h"
#include <chrono>

namespace dmq {
    struct FreeRTOSClock {
        // 1. Define duration traits (assuming 1 tick = 1 millisecond)
        // If configTICK_RATE_HZ is 1000, period is 1/1000 (std::milli)
        using rep = int64_t;
        using period = std::milli;
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<FreeRTOSClock>;
        static const bool is_steady = true;

        // 2. The critical "now()" function
        static time_point now() noexcept {
            // xTaskGetTickCount returns TickType_t (uint32_t)
            // We cast to int64_t to prevent overflow issues in math
            return time_point(duration(xTaskGetTickCount()));
        }
    };
}

#endif