#ifndef DMQ_DATABUSQOS_H
#define DMQ_DATABUSQOS_H

namespace dmq {

// Quality of Service settings for a topic.
struct QoS {
    bool lastValueCache = false; // If true, new subscribers receive the last published value.
};

} // namespace dmq

#endif // DMQ_DATABUSQOS_H
