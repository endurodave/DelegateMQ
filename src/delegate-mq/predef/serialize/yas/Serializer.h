#ifndef SERIALIZER_YAS_H
#define SERIALIZER_YAS_H

#include "delegate/ISerializer.h"
#include <yas/binary_iarchive.hpp>
#include <yas/binary_oarchive.hpp>
#include <yas/std_types.hpp>
#include <sstream>
#include <iostream>
#include <type_traits>

// Type trait to check if a type is const
template <typename T>
using is_const_type = std::is_const<std::remove_reference_t<T>>;

template <class R>
struct Serializer; // Not defined

template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public dmq::ISerializer<RetType(Args...)>
{
public:
    static constexpr std::size_t flags = yas::binary | yas::no_header;

    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        try {
            os.seekp(0);
            yas::binary_oarchive<std::ostream, flags> oa(os);  // <-- FIXED
            oa(std::forward<Args>(args)...);
        }
        catch (const std::exception& e) {
            std::cerr << "YAS serialize error: " << e.what() << std::endl;
            throw;
        }
        return os;
    }

    virtual std::istream& Read(std::istream& is, Args&... args) override {
        try {
            yas::binary_iarchive<std::istream, flags> ia(is);  // <-- FIXED
            ia(args...);
        }
        catch (const std::exception& e) {
            std::cerr << "YAS deserialize error: " << e.what() << std::endl;
            throw;
        }
        return is;
    }
};

#endif // SERIALIZER_YAS_H
