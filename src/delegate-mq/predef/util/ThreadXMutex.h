#ifndef THREADX_MUTEX_H
#define THREADX_MUTEX_H

#include "tx_api.h"

namespace dmq {

    // =========================================================================
    // ThreadXMutex 
    // Wraps TX_MUTEX.
    // Note: ThreadX mutexes support priority inheritance by default (TX_INHERIT).
    // =========================================================================
    class ThreadXMutex {
    public:
        ThreadXMutex() {
            // Create mutex with Priority Inheritance enabled
            UINT status = tx_mutex_create(&m_mutex, (CHAR*)"DMQ_Mutex", TX_INHERIT);
            if (status != TX_SUCCESS) {
                // Handle error (e.g., infinite loop or assert)
                while(1); 
            }
        }

        ~ThreadXMutex() {
            tx_mutex_delete(&m_mutex);
        }

        void lock() {
            tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);
        }

        void unlock() {
            tx_mutex_put(&m_mutex);
        }

        // Delete copy/move
        ThreadXMutex(const ThreadXMutex&) = delete;
        ThreadXMutex& operator=(const ThreadXMutex&) = delete;

    private:
        TX_MUTEX m_mutex;
    };

    // =========================================================================
    // ThreadXRecursiveMutex
    // Wraps TX_MUTEX (ThreadX mutexes are recursive by default)
    // =========================================================================
    class ThreadXRecursiveMutex {
    public:
        ThreadXRecursiveMutex() {
            UINT status = tx_mutex_create(&m_mutex, (CHAR*)"DMQ_RecMutex", TX_INHERIT);
            if (status != TX_SUCCESS) {
                while(1);
            }
        }

        ~ThreadXRecursiveMutex() {
            tx_mutex_delete(&m_mutex);
        }

        void lock() {
            tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);
        }

        void unlock() {
            tx_mutex_put(&m_mutex);
        }

        ThreadXRecursiveMutex(const ThreadXRecursiveMutex&) = delete;
        ThreadXRecursiveMutex& operator=(const ThreadXRecursiveMutex&) = delete;

    private:
        TX_MUTEX m_mutex;
    };
}

#endif // THREADX_MUTEX_H