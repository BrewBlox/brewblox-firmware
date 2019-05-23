#pragma once

// workaround for std::to_string not being available on arm
#include <sstream>
namespace std {
template <typename T>
string
to_string(const T& n)
{
    ostringstream s;
    s << n;
    return s.str();
}
}

#include "../cnl/include/cnl/elastic_integer.h"
#include "../cnl/include/cnl/fixed_point.h"
#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/overflow_integer.h"

#include <cstdint>
#include <type_traits>

template <
    int IntegerDigits,
    int FractionalDigits,
    class Narrowest>
using saturated_elastic_fixed_point = cnl::fixed_point<
    cnl::overflow_integer<
        cnl::elastic_integer<
            IntegerDigits + FractionalDigits,
            Narrowest>,
        cnl::saturated_overflow_tag>,
    -FractionalDigits>;

using fp12_t = saturated_elastic_fixed_point<11, 12, std::int32_t>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);