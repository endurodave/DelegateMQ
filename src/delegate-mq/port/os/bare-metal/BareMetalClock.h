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
            // On 32-bit ARM, reading a 64-bit value requires two 32-bit loads.
            // A SysTick ISR firing between them would produce a torn read.
            // Disable interrupts for the duration of the read to ensure atomicity.
#if defined(__GNUC__) || defined(__clang__)
            __asm__ volatile("cpsid i" ::: "memory");
            uint64_t t = g_ticks;
            __asm__ volatile("cpsie i" ::: "memory");
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
            __disable_irq();
            uint64_t t = g_ticks;
            __enable_irq();
#else
            // Fallback: plain read (safe on platforms with atomic 64-bit loads)
            uint64_t t = g_ticks;
#endif
            return time_point(duration(static_cast<rep>(t)));
        }
    };
}

#endif // BARE_METAL_CLOCK_H