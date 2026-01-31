#ifndef THREADX_MUTEX_H
#define THREADX_MUTEX_H

#include "tx_api.h"

namespace dmq {

    // =========================================================================
    // ThreadXMutex 
    // Wraps TX_MUTEX.
    // Note 1: ThreadX mutexes support priority inheritance by default (TX_INHERIT).
    // Note 2: ThreadX mutexes are inherently recursive. The same thread can 
    //         lock the mutex multiple times without deadlocking.
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
    // Alias for ThreadXMutex because TX_MUTEX is recursive by design.
    // =========================================================================
    using ThreadXRecursiveMutex = ThreadXMutex;
}

#endif // THREADX_MUTEX_H