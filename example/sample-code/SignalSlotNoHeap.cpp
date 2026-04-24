/**
 * @file SignalSlotNoHeap.cpp
 * @brief Demonstrates minimal-heap Signal usage for embedded targets.
 *
 * @details
 * Two approaches are shown, both using `Signal<>` as a direct variable:
 *
 * **Approach 1 — Signal as a plain member variable.**
 * `Signal` owns a single small heap-allocated lifetime token (`shared_ptr<void>`),
 * but the Signal object itself requires no external heap management. This covers
 * the majority of embedded signal/slot use cases with minimal heap impact.
 * With `DMQ_ALLOCATOR` enabled, the fixed-block allocator replaces the system heap
 * entirely for all internal delegate library allocations.
 *
 * **Approach 2 — Signal as a static variable.**
 * `Signal<>` is inherently thread-safe and can be declared as a plain static
 * variable. No shared_ptr wrapper or allocate_shared is needed. This is
 * equivalent to Approach 1 but with a static lifetime.
 *
 * To ensure the internal delegate list also avoids the system heap, compile with
 * `DMQ_ALLOCATOR` to enable the fixed-block allocator for all library containers.
 *
 * @see https://github.com/DelegateMQ/DelegateMQ
 */

#include "SignalSlotNoHeap.h"
#include "DelegateMQ.h"
#include <iostream>

using namespace dmq;
using namespace dmq::os;
using namespace dmq::util;
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
    // APPROACH 2: Thread-safe Signal as a static variable
    // Signal<> is inherently thread-safe — no shared_ptr or allocate_shared
    // required. Declare as a plain static variable for static lifetime.
    // ============================================================================
    void SignalSlotNoHeapExample()
    {
        cout << "\n=== Approach 2: Signal as Static Variable ===" << endl;

        // Static Signal — lives in BSS/data section, no heap allocation needed
        // for the Signal object itself.
        static Signal<void(int)> mySignal;

        cout << "[Info] Signal at static address: " << (void*)&mySignal << endl;

        // Connect a static function pointer — zero additional heap allocation.
        auto conn = mySignal.Connect(MakeDelegate(&MySubscriber));

        cout << "[Main] Broadcasting 42..." << endl;
        mySignal(42);

        cout << "=== Done ===" << endl;
    }

} // namespace Example
