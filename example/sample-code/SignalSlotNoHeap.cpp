/**
 * @file SignalSlotNoHeap.cpp
 * @brief Demonstrates minimal-heap and zero-heap Signal usage for embedded targets.
 *
 * @details
 * Two approaches are shown depending on whether thread-safe disconnect is required:
 *
 * **Approach 1 — Single-threaded: use `Signal` as a plain member variable.**
 * `Signal` owns a single small heap-allocated lifetime token (`shared_ptr<void>`),
 * but the Signal object itself requires no external heap management. This covers
 * the majority of embedded signal/slot use cases with minimal heap impact.
 * With `DMQ_ALLOCATOR` enabled, the fixed-block allocator replaces the system heap
 * entirely for all internal delegate library allocations.
 *
 * **Approach 2 — Thread-safe: use `SignalSafe` with a static allocator.**
 * `SignalSafe` must be managed by `std::shared_ptr` for thread-safe disconnect.
 * To avoid placing the Signal and its shared_ptr control block on the system heap,
 * use `std::allocate_shared` with a `StaticAllocator` backed by a static buffer
 * in the BSS section. This ensures all Signal memory is in known static storage.
 *
 * To ensure the internal delegate list also avoids the system heap, compile with
 * `DMQ_ALLOCATOR` to enable the fixed-block allocator for all library containers.
 *
 * @see https://github.com/endurodave/DelegateMQ
 */

#include "SignalSlotNoHeap.h"
#include "DelegateMQ.h"
#include <iostream>
#include <memory>
#include <cstdint>

using namespace dmq;
using namespace std;

namespace Example
{
    // ============================================================================
    // SHARED SUBSCRIBER FUNCTION
    // A static free function guarantees MakeDelegate uses a function pointer
    // with no additional heap allocation.
    // ============================================================================
    static void MySubscriber(int value) {
        cout << "[Subscriber] Received value: " << value << endl;
    }


    // ============================================================================
    // APPROACH 1: Single-threaded Signal as a plain class member
    // No shared_ptr management required. Signal can live on the stack or as a
    // member variable. Only the internal m_lifetime token touches the heap.
    // ============================================================================
    void SignalSlotMemberExample()
    {
        cout << "\n=== Approach 1: Signal as Plain Member Variable ===" << endl;

        // Declare Signal directly — no make_shared, no shared_ptr.
        Signal<void(int)> mySignal;

        cout << "[Info] Signal at address: " << (void*)&mySignal << endl;

        // Connect and fire
        ScopedConnection conn = mySignal.Connect(MakeDelegate(&MySubscriber));

        cout << "[Main] Broadcasting 10..." << endl;
        mySignal(10);

        conn.Disconnect();
        mySignal(99); // Not heard — safely ignored.

        cout << "=== Done ===" << endl;
    }


    // ============================================================================
    // APPROACH 2: Thread-safe SignalSafe in static memory
    // Required when Disconnect() may be called from a thread other than the one
    // that owns the Signal. Uses std::allocate_shared with a StaticAllocator to
    // place the SignalSafe object and its shared_ptr control block in static BSS
    // memory instead of the system heap.
    // ============================================================================

    template <typename T>
    struct StaticAllocator {
        using value_type = T;
        uint8_t* m_buffer;
        size_t   m_capacity;

        StaticAllocator(uint8_t* buffer, size_t capacity)
            : m_buffer(buffer), m_capacity(capacity) {}

        template <typename U>
        StaticAllocator(const StaticAllocator<U>& other)
            : m_buffer(other.m_buffer), m_capacity(other.m_capacity) {}

        T* allocate(std::size_t n) {
            if (n * sizeof(T) > m_capacity) std::terminate();
            return reinterpret_cast<T*>(m_buffer);
        }

        void deallocate(T*, std::size_t) {}
    };

    template <typename T, typename U>
    bool operator==(const StaticAllocator<T>&, const StaticAllocator<U>&) { return true; }
    template <typename T, typename U>
    bool operator!=(const StaticAllocator<T>&, const StaticAllocator<U>&) { return false; }

    void SignalSlotNoHeapExample()
    {
        cout << "\n=== Approach 2: Thread-Safe SignalSafe in Static Memory ===" << endl;

        // Static buffer in BSS — no system heap involved for the Signal object.
        using MySignal = SignalSafe<void(int)>;
        alignas(std::max_align_t) static uint8_t buffer[sizeof(MySignal) + 64];

        // allocate_shared places both the SignalSafe object and the shared_ptr
        // control block inside 'buffer' using the StaticAllocator.
        auto mySignal = std::allocate_shared<MySignal>(
            StaticAllocator<MySignal>(buffer, sizeof(buffer))
        );

        cout << "[Info] SignalSafe at static address: " << (void*)mySignal.get() << endl;

        // Connect a static function pointer — zero additional heap allocation.
        auto conn = mySignal->Connect(MakeDelegate(&MySubscriber));

        cout << "[Main] Broadcasting 42..." << endl;
        (*mySignal)(42);

        cout << "=== Done ===" << endl;
    }

} // namespace Example
