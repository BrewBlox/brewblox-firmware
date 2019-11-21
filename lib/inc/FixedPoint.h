#pragma once

#ifdef __arm__
#include <string>

// forward declare std::to_string. Arm gcc 5.3 compiler cannot find it in headers and cnl has references to it in headers
namespace std {
template <typename T>
string to_string(T);
}
#endif

#define CNL_USE_INT128 false
#define CNL_RELEASE true

#include "../cnl/include/cnl/num_traits.h"
#include "../cnl/include/cnl/static_number.h"
#include <cstdint>

template <
    int Digits,
    int Exponent,
    typename Narrowest = signed>
using safe_elastic_fixed_point = cnl::static_number<Digits, Exponent, cnl::native_rounding_tag, cnl::saturated_overflow_tag, Narrowest>;

using fp12_t = safe_elastic_fixed_point<23, -12>;

std::string
to_string_dec(const fp12_t& t, uint8_t decimals);
