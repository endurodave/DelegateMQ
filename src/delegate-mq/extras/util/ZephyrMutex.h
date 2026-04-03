#ifndef ZEPHYR_MUTEX_H
#define ZEPHYR_MUTEX_H

#include <zephyr/kernel.h>

namespace dmq {

    // =========================================================================
    // ZephyrMutex
    // Wraps k_mutex. 
    // =========================================================================
    class ZephyrMutex {
    public:
        ZephyrMutex() {
            k_mutex_init(&m_mutex);
        }

        ~ZephyrMutex() { }

        void lock() {
            k_mutex_lock(&m_mutex, K_FOREVER);
        }

        // REQUIRED for std::unique_lock compatibility
        bool try_lock() {
            return k_mutex_lock(&m_mutex, K_NO_WAIT) == 0;
        }

        void unlock() {
            k_mutex_unlock(&m_mutex);
        }

        ZephyrMutex(const ZephyrMutex&) = delete;
        ZephyrMutex& operator=(const ZephyrMutex&) = delete;

    private:
        struct k_mutex m_mutex;
    };

    using ZephyrRecursiveMutex = ZephyrMutex;
}

#endif // ZEPHYR_MUTEX_H