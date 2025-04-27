#ifndef DATA_H
#define DATA_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Bitsery-serialized data classes for transport using remote delegates.

#include <bitsery/bitsery.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/traits/string.h> 
#include <string>
#include <vector>

class DataPoint {
public:
    int x = 0;
    int y = 0;
};

template <typename S>
void serialize(S& s, DataPoint& d) {
    s.value4b(d.x);
    s.value4b(d.y);
}

class Data {
public:
    std::vector<DataPoint> dataPoints;
    std::string msg;
};

template <typename S>
void serialize(S& s, Data& d) {
    s.container(d.dataPoints, 100);  // Vector, max size hint
    s.text1b(d.msg, 255);            // String serialization
}

#endif