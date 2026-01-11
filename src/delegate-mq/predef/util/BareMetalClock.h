#ifndef BARE_METAL_CLOCK_H
#define BARE_METAL_CLOCK_H

#include <chrono>
#include <cstdint>

// 1. Declare the external tick counter.
// This variable must be defined in your main.cpp or startup.c
// and incremented by your SysTick_Handler (or other timer ISR).
extern "C" volatile uint64_t g_ticks;

namespace dmq {

    struct BareMetalClock {
        // 2. Define duration traits
        // We assume g_ticks represents Milliseconds (1/1000 sec).
        // If your timer runs at a different rate (e.g. Microseconds), change std::milli to std::micro.
        using rep = int64_t;
        using period = std::milli;
        using duration = std::chrono::duration<rep, period>;
        using time_point = std::chrono::time_point<BareMetalClock>;

        static const bool is_steady = true;

        // 3. The critical "now()" function
        static time_point now() noexcept {
            // Read the volatile global variable directly.
            // Note: On 32-bit ARM, reading a 64-bit volatile is technically not atomic.
            // For a rock-solid production system, you might need a "read_atomic()" helper 
            // that disables interrupts briefly.
            return time_point(duration(g_ticks));
        }
    };
}

#endif // BARE_METAL_CLOCK_H