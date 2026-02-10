#ifndef CMSIS_RTOS2_MUTEX_H
#define CMSIS_RTOS2_MUTEX_H

#include "cmsis_os2.h"
#include <cassert>

namespace dmq {

    // =========================================================================
    // CmsisRtos2Mutex (Non-Recursive)
    // =========================================================================
    class CmsisRtos2Mutex {
    public:
        CmsisRtos2Mutex() {
            // Explicitly define attributes as 0 (Non-Recursive)
            osMutexAttr_t attr = {0};
            attr.attr_bits = 0; 
            m_id = osMutexNew(&attr);
            assert(m_id != NULL);
        }

        ~CmsisRtos2Mutex() {
            if (m_id) osMutexDelete(m_id);
        }

        void lock() {
            if (m_id) osMutexAcquire(m_id, osWaitForever);
        }

        // REQUIRED for std::unique_lock compatibility
        bool try_lock() {
            if (m_id) {
                return osMutexAcquire(m_id, 0) == osOK;
            }
            return false;
        }

        void unlock() {
            if (m_id) osMutexRelease(m_id);
        }

        CmsisRtos2Mutex(const CmsisRtos2Mutex&) = delete;
        CmsisRtos2Mutex& operator=(const CmsisRtos2Mutex&) = delete;

    private:
        osMutexId_t m_id = NULL;
    };

    // =========================================================================
    // CmsisRtos2RecursiveMutex (Recursive)
    // =========================================================================
    class CmsisRtos2RecursiveMutex {
    public:
        CmsisRtos2RecursiveMutex() {
            osMutexAttr_t attr = {0};
            attr.attr_bits = osMutexRecursive; 
            m_id = osMutexNew(&attr);
            assert(m_id != NULL);
        }

        ~CmsisRtos2RecursiveMutex() {
            if (m_id) osMutexDelete(m_id);
        }

        void lock() {
            if (m_id) osMutexAcquire(m_id, osWaitForever);
        }

        bool try_lock() {
            if (m_id) {
                return osMutexAcquire(m_id, 0) == osOK;
            }
            return false;
        }

        void unlock() {
            if (m_id) osMutexRelease(m_id);
        }

        CmsisRtos2RecursiveMutex(const CmsisRtos2RecursiveMutex&) = delete;
        CmsisRtos2RecursiveMutex& operator=(const CmsisRtos2RecursiveMutex&) = delete;

    private:
        osMutexId_t m_id = NULL;
    };
}

#endif // CMSIS_RTOS2_MUTEX_H