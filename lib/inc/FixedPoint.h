#pragma once

// workaround for std::to_string not being available on arm

#include <sstream>
namespace std {

template <typename T>
std::string
to_string(const T& n)
{
    std::ostringstream s;
    s << n;
    return s.str();
}
}

#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/static_number.h"

#include <cstdint>
#include <type_traits>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest>
using saturated_elastic_fixed_point = cnl::static_number<
    IntegerDigits + FractionalDigits, -FractionalDigits, cnl::native_rounding_tag, cnl::saturated_overflow_tag, Narrowest>;

using fp12_t = saturated_elastic_fixed_point<11, 12, std::int32_t>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);