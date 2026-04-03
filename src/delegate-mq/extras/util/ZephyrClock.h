#ifndef ZEPHYR_CLOCK_H
#define ZEPHYR_CLOCK_H

#include <zephyr/kernel.h>
#include <chrono>

namespace dmq {
    struct ZephyrClock {
        // Zephyr k_uptime_get() returns milliseconds (int64_t)
        using rep = int64_t;
        using period = std::milli;
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<ZephyrClock>;
        static const bool is_steady = true;

        static time_point now() noexcept {
            return time_point(duration(k_uptime_get()));
        }
    };
}

#endif // ZEPHYR_CLOCK_H