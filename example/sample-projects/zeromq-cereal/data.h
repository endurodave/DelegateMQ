#ifndef DATA_H
#define DATA_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Cereal-serialized data classes for transport using remote delegates.

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>
#include <string>
#include <vector>

class DataPoint {
public:
    int x = 0;
    int y = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(x, y);
    }
};

/// @brief Argument data sent to endpoint.
class Data {
public:
    std::vector<DataPoint> dataPoints;
    std::string msg;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(dataPoints, msg);
    }
};

#endif
