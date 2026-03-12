#ifndef SIGNAL_SLOT_NO_HEAP_H
#define SIGNAL_SLOT_NO_HEAP_H

namespace Example
{
    /// @brief Demonstrates Signal as a plain member variable (single-threaded, minimal heap).
    void SignalSlotMemberExample();

    /// @brief Demonstrates Signal as a static variable (thread-safe, no heap management needed).
    void SignalSlotNoHeapExample();
}

#endif // SIGNAL_SLOT_NO_HEAP_H