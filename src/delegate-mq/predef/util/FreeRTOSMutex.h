#ifndef FREERTOS_MUTEX_H
#define FREERTOS_MUTEX_H

#include "FreeRTOS.h"
#include "semphr.h"

namespace dmq {

    // =========================================================================
    // FreeRTOSMutex (Non-Recursive)
    // Matches std::mutex behavior
    // =========================================================================
    class FreeRTOSMutex {
    public:
        FreeRTOSMutex() {
            // Standard Mutex: Cannot be taken recursively by the same thread.
            m_handle = xSemaphoreCreateMutex();
            configASSERT(m_handle);
        }

        ~FreeRTOSMutex() {
            if (m_handle) {
                vSemaphoreDelete(m_handle);
            }
        }

        void lock() {
            if (m_handle) {
                xSemaphoreTake(m_handle, portMAX_DELAY);
            }
        }

        void unlock() {
            if (m_handle) {
                xSemaphoreGive(m_handle);
            }
        }

        FreeRTOSMutex(const FreeRTOSMutex&) = delete;
        FreeRTOSMutex& operator=(const FreeRTOSMutex&) = delete;

    private:
        SemaphoreHandle_t m_handle = nullptr;
    };

    // =========================================================================
    // FreeRTOSRecursiveMutex (Recursive)
    // Matches std::recursive_mutex behavior
    // =========================================================================
    class FreeRTOSRecursiveMutex {
    public:
        FreeRTOSRecursiveMutex() {
            // Recursive Mutex: Can be taken multiple times by the same thread.
            // Requires configUSE_RECURSIVE_MUTEXES == 1 in FreeRTOSConfig.h
            m_handle = xSemaphoreCreateRecursiveMutex();
            configASSERT(m_handle);
        }

        ~FreeRTOSRecursiveMutex() {
            if (m_handle) {
                vSemaphoreDelete(m_handle);
            }
        }

        void lock() {
            if (m_handle) {
                xSemaphoreTakeRecursive(m_handle, portMAX_DELAY);
            }
        }

        void unlock() {
            if (m_handle) {
                xSemaphoreGiveRecursive(m_handle);
            }
        }

        FreeRTOSRecursiveMutex(const FreeRTOSRecursiveMutex&) = delete;
        FreeRTOSRecursiveMutex& operator=(const FreeRTOSRecursiveMutex&) = delete;

    private:
        SemaphoreHandle_t m_handle = nullptr;
    };

}

#endif // FREERTOS_MUTEX_H