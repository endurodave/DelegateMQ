#include "data.h"

using namespace std;

// Simple serialization of Data. Use any serialization scheme as necessary. 
std::ostream& operator<<(std::ostream& os, const Data& data) {
    os << data.x << endl;
    os << data.y << endl;
    os << data.msg << endl;
    return os;
}

// Simple unserialization of Data. Use any unserialization scheme as necessary. 
std::istream& operator>>(std::istream& is, Data& data) {
    is >> data.x;
    is >> data.y;
    is >> data.msg;
    return is;
}
