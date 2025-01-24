#ifndef DATA_H
#define DATA_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include <string.h>
#include <iostream>

/// @brief Argument data sent to endpoint.
class Data
{
public:
    int x;
    int y;
    std::string msg;
};

std::ostream& operator<<(std::ostream& os, const Data& data);
std::istream& operator>>(std::istream& is, Data& data);

#endif
