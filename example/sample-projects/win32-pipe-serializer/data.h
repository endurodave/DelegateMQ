#ifndef DATA_H
#define DATA_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include <string.h>
#include <iostream>

class DataPoint : public serialize::I
{
public:
    int x = 0;
    int y = 0;

    virtual std::ostream& write(serialize& ms, std::ostream& os) override;
    virtual std::istream& read(serialize& ms, std::istream& is) override;
};

/// @brief Argument data sent to endpoint.
class Data : public serialize::I
{
public:
    std::vector<DataPoint> dataPoints;
    std::string msg;

    virtual std::ostream& write(serialize& ms, std::ostream& os) override;
    virtual std::istream& read(serialize& ms, std::istream& is) override;
};

class DataAux : public serialize::I
{
public:
    std::string auxMsg;

    virtual std::ostream& write(serialize& ms, std::ostream& os) override;
    virtual std::istream& read(serialize& ms, std::istream& is) override;
};

#endif
