#ifndef ZEPHYR_MUTEX_H
#define ZEPHYR_MUTEX_H

#include <zephyr/kernel.h>

namespace dmq {

    // =========================================================================
    // ZephyrMutex
    // Wraps k_mutex. Zephyr mutexes are recursive by default.
    // =========================================================================
    class ZephyrMutex {
    public:
        ZephyrMutex() {
            k_mutex_init(&m_mutex);
        }

        ~ZephyrMutex() {
            // Zephyr does not have a k_mutex_destroy/delete API
        }

        void lock() {
            k_mutex_lock(&m_mutex, K_FOREVER);
        }

        void unlock() {
            k_mutex_unlock(&m_mutex);
        }

        ZephyrMutex(const ZephyrMutex&) = delete;
        ZephyrMutex& operator=(const ZephyrMutex&) = delete;

    private:
        struct k_mutex m_mutex;
    };

    // Alias for RecursiveMutex since k_mutex supports it natively
    using ZephyrRecursiveMutex = ZephyrMutex;
}

#endif // ZEPHYR_MUTEX_H