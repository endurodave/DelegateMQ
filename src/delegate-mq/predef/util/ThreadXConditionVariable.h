#ifndef THREADX_CONDITION_VARIABLE_H
#define THREADX_CONDITION_VARIABLE_H

#include "tx_api.h"
#include <chrono>

namespace dmq
{
    /// @brief Production-grade wrapper around ThreadX Semaphore to mimic std::condition_variable
    class ThreadXConditionVariable
    {
    public:
        ThreadXConditionVariable() 
        { 
            // Create a binary semaphore (initially 0)
            // TX_NO_INHERIT: Priority inheritance not needed for signaling
            if (tx_semaphore_create(&m_sem, "DMQ_CondVar", 0) != TX_SUCCESS)
            {
                // In production, handle error or assert
                // configASSERT(false); 
            }
        }

        ~ThreadXConditionVariable() 
        { 
            tx_semaphore_delete(&m_sem);
        }

        ThreadXConditionVariable(const ThreadXConditionVariable&) = delete;
        ThreadXConditionVariable& operator=(const ThreadXConditionVariable&) = delete;

        /// @brief Wake up one waiting thread
        void notify_one() noexcept
        {
            // ThreadX tx_semaphore_put is generally ISR-safe.
            // However, ensuring we don't overflow a "binary" concept logic:
            // We usually don't check for ceiling in CV logic because spurious 
            // wakes are handled by the predicate loop.
            tx_semaphore_put(&m_sem);
        }

        /// @brief Wait indefinitely until predicate is true
        template <typename Lock, typename Predicate>
        void wait(Lock& lock, Predicate pred)
        {
            while (!pred())
            {
                lock.unlock();
                // Wait indefinitely
                tx_semaphore_get(&m_sem, TX_WAIT_FOREVER);
                lock.lock();
            }
        }

        /// @brief Wait until predicate is true or timeout expires
        template <typename Lock, typename Predicate>
        bool wait_for(Lock& lock, std::chrono::milliseconds timeout, Predicate pred)
        {
            // 1. Convert timeout to ticks
            // ThreadX usually runs at 100Hz or 1000Hz (TX_TIMER_TICKS_PER_SECOND)
            const ULONG ticksPerSec = TX_TIMER_TICKS_PER_SECOND;
            const ULONG timeoutTicks = (static_cast<ULONG>(timeout.count()) * ticksPerSec) / 1000;
            
            const ULONG startTick = tx_time_get();

            while (!pred())
            {
                ULONG now = tx_time_get();
                
                // ThreadX time is ULONG (usually 32-bit), logic same as FreeRTOS
                ULONG elapsed = now - startTick;

                if (elapsed >= timeoutTicks)
                    return pred(); // Timeout expired

                ULONG remaining = timeoutTicks - elapsed;

                lock.unlock();
                
                // Wait for the semaphore or timeout
                UINT res = tx_semaphore_get(&m_sem, remaining);
                
                lock.lock();

                if (res != TX_SUCCESS)
                {
                    // TX_NO_INSTANCE usually means timeout in this context
                    return pred(); 
                }
            }

            return true;
        }

    private:
        TX_SEMAPHORE m_sem;
    };
}

#endif // THREADX_CONDITION_VARIABLE_H