/**
 * @file SignalSlotNoHeap.cpp
 * @brief Demonstrates "Zero-Heap" Signal creation using std::allocate_shared.
 *
 * @details
 * This example solves a specific problem in embedded C++: using `std::shared_ptr`
 * (which is required for DelegateMQ's RAII connection management) without
 * triggering dynamic memory allocations on the System Heap.
 *
 * **The Problem:**
 * - `dmq::SignalSafe` requires `std::shared_ptr` to support `weak_ptr` for safe
 * disconnection handles.
 * - Standard `std::make_shared` allocates the Control Block and the Object
 * on the heap, causing potential fragmentation.
 *
 * **The Solution:**
 * - We define a custom `StaticAllocator` that redirects allocation requests
 * to a pre-allocated static buffer (BSS section).
 * - We use `std::allocate_shared` to construct the Signal and its Reference
 * Counter inside this static buffer.
 *
 * **Note:**
 * This technique strictly controls where the *Signal Object* lives. To ensure
 * the internal slot list (std::vector/list nodes) also avoids the heap,
 * you must compile with `DMQ_ALLOCATOR` defined to use the Fixed-Block Allocator.
 *
 * @see https://github.com/endurodave/DelegateMQ
 */

#include "SignalSlotNoHeap.h"
#include "DelegateMQ.h"
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <functional> // Required if you use std::function

using namespace dmq;
using namespace std;

namespace Example
{
    // ============================================================================
    // 1. STATIC ALLOCATOR
    // ============================================================================
    template <typename T>
    struct StaticAllocator {
        using value_type = T;
        uint8_t* m_buffer;
        size_t m_capacity;

        StaticAllocator(uint8_t* buffer, size_t capacity)
            : m_buffer(buffer), m_capacity(capacity) {
        }

        template <typename U>
        StaticAllocator(const StaticAllocator<U>& other)
            : m_buffer(other.m_buffer), m_capacity(other.m_capacity) {
        }

        T* allocate(std::size_t n) {
            if (n * sizeof(T) > m_capacity) std::terminate();
            return reinterpret_cast<T*>(m_buffer);
        }

        void deallocate(T* p, std::size_t n) {}
    };

    template <typename T, typename U> bool operator==(const StaticAllocator<T>&, const StaticAllocator<U>&) { return true; }
    template <typename T, typename U> bool operator!=(const StaticAllocator<T>&, const StaticAllocator<U>&) { return false; }

    // ============================================================================
    // 2. SUBSCRIBER FUNCTION (Zero Heap)
    // ============================================================================
    // Moving the logic to a static function is cleaner and guarantees 
    // MakeDelegate binds to a function pointer (no heap allocation).
    static void MySubscriber(int value) {
        cout << "[Subscriber] Received value: " << value << endl;
    }

    // ============================================================================
    // 3. MAIN EXAMPLE
    // ============================================================================
    void SignalSlotNoHeapExample()
    {
        cout << "=== Zero-Heap Signal Example ===" << endl;

        // 1. Define Buffer
        using MySignal = SignalSafe<void(int)>;
        alignas(std::max_align_t) static uint8_t buffer[sizeof(MySignal) + 64];

        // 2. Create Signal (Static Allocator)
        auto mySignal = std::allocate_shared<MySignal>(
            StaticAllocator<MySignal>(buffer, sizeof(buffer))
        );

        cout << "[Info] Signal created at static address: " << (void*)mySignal.get() << endl;

        // 3. Connect Subscriber
        // Use a static function pointer. 
        // This is strictly zero-heap and MakeDelegate handles it perfectly.
        auto conn = mySignal->Connect(MakeDelegate(&MySubscriber));

        /* ALTERNATIVE (If you MUST use lambda):
           You must explicitly cast to std::function, but this MIGHT allocate heap
           depending on your STL implementation.

           auto conn = mySignal->Connect(MakeDelegate(
               std::function<void(int)>([](int v) { cout << v; })
           ));
        */

        // 4. Invoke
        cout << "[Main] Broadcasting 42..." << endl;
        (*mySignal)(42);

        cout << "=== Done ===" << endl;
    }
}