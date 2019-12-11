#pragma once

#ifdef __arm__
#include <string>

// forward declare std::to_string. Arm gcc 5.3 compiler cannot find it in headers and cnl has references to it in headers
namespace std {
template <typename T>
string to_string(T);
}
#define CNL_RELEASE true
#endif

#define CNL_USE_INT128 false

#include "../cnl/include/cnl/elastic_integer.h"
#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/overflow_integer.h"
#include "../cnl/include/cnl/rounding_integer.h"
#include "../cnl/include/cnl/scaled_integer.h"
#include <cstdint>

template <
    int Digits,
    int Exponent>
using safe_elastic_fixed_point = cnl::scaled_integer<
    cnl::overflow_integer<
        cnl::elastic_integer<
            Digits,
            cnl::rounding_integer<
                int32_t,
                cnl::nearest_rounding_tag>>,
        cnl::saturated_overflow_tag>,
    cnl::power<Exponent>>;

using fp12_t = safe_elastic_fixed_point<23, -12>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);
