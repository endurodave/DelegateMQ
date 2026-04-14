#ifndef DMQ_DATABUSQOS_H
#define DMQ_DATABUSQOS_H

#include "DelegateMQ.h"
#include <optional>

namespace dmq {

// Quality of Service settings for a topic subscription.
struct QoS {
    // If true, new subscribers receive the last published value immediately on subscribe.
    bool lastValueCache = false;

    // Maximum age of a cached LVC value. If the cached value is older than this duration
    // when a new subscriber connects, it is considered stale and not delivered.
    // Only meaningful when lastValueCache = true.
    std::optional<dmq::Duration> lifespan;

    // Minimum time between deliveries to this subscriber. Publishes that arrive faster
    // than this interval are silently dropped for this subscriber only. Other subscribers
    // with a different (or no) minSeparation are unaffected.
    std::optional<dmq::Duration> minSeparation;
};

} // namespace dmq

#endif // DMQ_DATABUSQOS_H
